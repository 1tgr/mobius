/*****************************************************************************
serial mouse driver
*****************************************************************************/

/*
 * Based on code written by Chris Giese (geezer@execpc.com):
 *  http://www.execpc.com/~geezer/os/
 */

#include <kernel/kernel.h>
#include <kernel/driver.h>

#include <os/mouse.h>

#include <errno.h>

/*#define DEBUG*/
#include <kernel/debug.h>

typedef struct sermouse_t sermouse_t;
struct sermouse_t
{
    device_t *device;
    uint16_t iobase;
    uint8_t irq;
    unsigned bytes;
    uint8_t data[4];
};

/*
 *          D7      D6      D5      D4      D3      D2      D1      D0
 *  1.      X       1       LB      RB      Y7      Y6      X7      X6
 *  2.      X       0       X5      X4      X3      X2      X1      X0
 *  3.      X       0       Y5      Y4      Y3      Y2      Y1      Y0
 */

void MseStartIo(sermouse_t *mouse)
{
    uint32_t but;
    int32_t dx, dy;
    asyncio_t *io, *next;
    mouse_packet_t* pkt;

	if (mouse->bytes < 3)
		return;

    but = 0;
    if (mouse->data[0] & 0x20)
        but |= MOUSE_BUTTON_LEFT;
    if (mouse->data[0] & 0x10)
        but |= MOUSE_BUTTON_RIGHT;

    dx = ((mouse->data[0] & 0x03) << 6) + mouse->data[1];
    dy = ((mouse->data[0] & 0x0c) << 4) + mouse->data[2];
	mouse->bytes = 0;

    if (dx >= 128)
        dx -= 256;
    if (dy >= 128)
        dy -= 256;

    if (dx > 5 || dx < -5)
	dx *= 4;
    if (dy > 5 || dy < -5)
	dy *= 4;

    TRACE2("sermouse: (%d, %d)\n", dx, dy);

    /* xxx -- SpinAcquire(&mouse->device->sem); */

    for (io = mouse->device->io_first; io != NULL; io = next)
    {
		next = io->next;
		pkt = (mouse_packet_t*) ((uint8_t*) MemMapPageArray(io->pages) + io->length);

		pkt->dx = dx;
		pkt->dy = dy;
		pkt->buttons = but;
		pkt->dwheel = 0;

		MemUnmapPageArray(io->pages);

		io->length += sizeof(*pkt);
		if (io->length >= ((request_dev_t*) io->req)->params.dev_read.length)
			DevFinishIo(mouse->device, io, 0);
    }

    /* xxx -- SpinRelease(&mouse->device->sem); */
}


static void MseDeleteDevice(device_t *device)
{
	sermouse_t *mouse = device->cookie;

	/* disable interrupts at serial chip */
	out(mouse->iobase + 1, 0);
	/* xxx -- DevDeregisterIrq(mouse->irq, mouse->device); */
	free(mouse);
}


static status_t MseRequest(device_t *device, request_t* req)
{
    sermouse_t *mouse = device->cookie;
    request_dev_t *req_dev;
    asyncio_t *io;

    TRACE2("{%c%c}", req->code / 256, req->code % 256);
        req_dev = (request_dev_t*) req;
    switch (req->code)
    {
    case DEV_READ:
        if (req_dev->params.dev_read.length < sizeof(mouse_packet_t))
        {
            TRACE1("%d: too small\n", req_dev->params.dev_read.length);
            return EBUFFER;
        }

        io = DevQueueRequest(device, 
            &req_dev->header, 
            sizeof(*req_dev), 
            req_dev->params.dev_read.pages,
            req_dev->params.dev_read.length);
        if (io == NULL)
            return errno;

        io->length = 0;
        MseStartIo(mouse);
        return SIOPENDING;
    }

    return ENOTIMPL;
}


static bool MseIsr(device_t *device, uint8_t irq)
{
    sermouse_t *mouse;
    uint8_t temp;

    mouse = device->cookie;
    temp = in(mouse->iobase  + 5);
    if (temp & 0x01)
    {
        temp = in(mouse->iobase + 0);
        TRACE1("%02X ", temp);

        if (temp & 0x40)
            mouse->bytes = 0;
		assert(mouse->bytes < _countof(mouse->data));
        mouse->data[mouse->bytes++] = temp;
    }

    in(mouse->iobase + 2);

    MseStartIo(mouse);
    return true;
}


static const device_vtbl_t sermouse_vtbl =
{
	MseDeleteDevice,
    MseRequest,
    MseIsr,
    NULL,       /* finishio */
};

/*****************************************************************************
*****************************************************************************/
#define MAX_ID  16

void MseAddDevice(driver_t *drv, const wchar_t *name, dev_config_t *cfg)
{
    static const uint8_t com_toirq[] =
    {
        4, 3, 4, 3
    };

    static const uint16_t io_port[] =
    {
        0x3f8, 0x2f8, 0x3f8, 0x2f8
    };

    unsigned char b, id[MAX_ID], *ptr;
    unsigned temp, com;
    sermouse_t *mouse;

    if (cfg->device_class == 0)
        cfg->device_class = 0x0902;

    mouse = malloc(sizeof(*mouse));
    mouse->bytes = 0;
    com = 1;

    if (com < 1 || com > 4)
    {
        TRACE1("Invalid COM port %u (must be 1-4)\n", com);
        return;
    }

    mouse->iobase = io_port[com - 1];
    if(mouse->iobase == 0)
    {
        TRACE1("Sorry, no COM%u serial port on this PC\n", com);
        return;
    }
    TRACE1("activating mouse on COM%u...\n", com);
    mouse->irq = com_toirq[com - 1];

	mouse->device = DevAddDevice(drv, &sermouse_vtbl, 0, name, cfg, mouse);

    /* install handler */
    DevRegisterIrq(mouse->irq, mouse->device);

    /*
     * set up serial chip
     * turn off handshaking lines to power-down mouse, then delay
     */
    out(mouse->iobase + 4, 0);
    ArchMicroDelay(500000);
    out(mouse->iobase + 3, 0x80);    /* set DLAB to access baud divisor */
    out(mouse->iobase + 1, 0);       /* 1200 baud */
    out(mouse->iobase + 0, 96);
    out(mouse->iobase + 3, 0x02);    /* clear DLAB bit, set 7N1 format */
    out(mouse->iobase + 2, 0);       /* turn off FIFO, if any */

    /* activate Out2, RTS, and DTR to power the mouse */
    out(mouse->iobase + 4, 0x0B);

    /* enable receiver interrupts */
    out(mouse->iobase + 1, 0x01);

    /*
     * Wait a moment to get ID bytes from mouse. Microsoft mice just return
     *  a single 'M' byte, some 3-button mice return "M3", my Logitech mouse
     *  returns a string of bytes. I could find no documentation on these,
     *  but I figured out some of it on my own.
     */
    ptr = id;
    for (temp = 750; temp != 0; temp--)
    {
        while (mouse->bytes > 0)
        {
            b = mouse->data[0];
            mouse->bytes = 0;
            //TRACE1("%02X ", b);
            *ptr = b;
            ptr++;
            if (ptr >= id + MAX_ID)
                goto got_id;
        }

        ArchMicroDelay(1000);
    }
    TRACE0("\n");

    if (id == ptr)
    {
        TRACE0("No serial mouse found\n");
        return;
    }

got_id:
    if(ptr >= id + 11)
    {
        /* find the 'M' */
        for (ptr = id; ptr < id + MAX_ID - 7; ptr++)
            if (*ptr == 'M')
                break;

        if(ptr < id + MAX_ID)
        {
            /*
             * four bytes that each encode 4 bits of the numeric portion
             *  of the PnP ID. Each byte has b4 set (i.e. ORed with 0x10)
             */
            temp = (ptr[8] & 0x0F);
            temp <<= 4;
            temp |= (ptr[9] & 0x0F);
            temp <<= 4;
            temp |= (ptr[10] & 0x0F);
            temp <<= 4;
            temp |= (ptr[11] & 0x0F);
            /*
             * three bytes that each encode one character of the character 
             *  portion of the PnP ID. Each byte has b5 set (i.e. ORed with 
             *  0x20)
             */
            TRACE4("mouse PnP ID: %c%c%c%04X\n",
                '@' + (ptr[5] & 0x1F),
                '@' + (ptr[6] & 0x1F),
                '@' + (ptr[7] & 0x1F), temp);
        }
        /*
         * example: Logitech serial mouse LGI8001
         *  ('L' - '@') | 0x20 == 0x2C
         *  ('G' - '@') | 0x20 == 0x27
         *  ('I' - '@') | 0x20 == 0x29
         *  ('8' - '0') | 0x10 == 0x18
         *  ('0' - '0') | 0x10 == 0x10
         *  ('0' - '0') | 0x10 == 0x10
         *  ('1' - '0') | 0x10 == 0x11
         */
    }
}

bool __initcode DrvInit(driver_t* drv)
{
    drv->add_device = MseAddDevice;
    return true;
}

//@}
