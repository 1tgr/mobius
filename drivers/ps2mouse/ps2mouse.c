#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/arch.h>

#define DEBUG
#include <kernel/debug.h>

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include <os/mouse.h>
#include <os/syscall.h>

/*!
 *  \ingroup	drivers
 *  \defgroup	ps2mouse    PS/2 aux port and mouse
 *  @{
 */

/*
 * General keyboard defines --
 *	needed for i8042 port (PS/2 controller)
 */

#define KEYB_PORT		    0x60    /* keyboard port */
#define KEYB_CTRL		    0x64    /* keyboard controller port */

#define KCTRL_ENABLE_AUX	    0xA8    /* enable aux port (PS/2 mouse) */
#define KCTRL_WRITE_CMD_BYTE	    0x60    /* write to command register */
#define KCTRL_WRITE_AUX		    0xD4    /* write next byte at port 60 to aux port */

/* flags for KCTRL_WRITE_CMD_BYTE */
#define KCTRL_IRQ1		    0x01
#define KCTRL_IRQ12		    0x02
#define KCTRL_SYS		    0x04
#define KCTRL_OVERRIDE_INHIBIT	    0x08
#define KCTRL_DISABLE_KEYB	    0x10
#define KCTRL_DISABLE_AUX	    0x20
#define KCTRL_TRANSLATE_XT	    0x40

/* commands to keyboard */
#define KEYB_SET_LEDS		    0xED
#define KEYB_SET_SCANCODE_SET	    0xF0
#define KEYB_IDENTIFY		    0xF2
#define KEYB_SET_TYPEMATIC	    0xF3
#define KEYB_ENABLE		    0xF4
#define KEYB_RESET_DISABLE	    0xF5
#define KEYB_ALL_TYPM_MAKE_BRK	    0xFA

/* default ACK from keyboard following command */
#define KEYB_ACK		    "\xFA"

/* commands to aux device (PS/2 mouse) */
#define AUX_INFORMATION		    0xE9
#define AUX_ENABLE		    0xF4
#define AUX_IDENTIFY		    0xF2
#define AUX_RESET		    0xFF

typedef struct Ps2Mouse Ps2Mouse;
struct Ps2Mouse
{
    device_t dev;
    bool has_wheel;
    unsigned bytes;
    uint8_t data[4];
};

void kbdWrite(uint16_t port, uint8_t data)
{
    uint32_t timeout;
    uint8_t stat;

    for (timeout = 500000L; timeout != 0; timeout--)
    {
	stat = in(KEYB_CTRL);

	if ((stat & 0x02) == 0)
	    break;
    }

    if (timeout != 0)
	out(port, data);
}

uint8_t kbdRead()
{
    unsigned long Timeout;
    uint8_t Stat, Data;

    for (Timeout = 50000L; Timeout != 0; Timeout--)
    {
	Stat = in(KEYB_CTRL);

	/* loop until 8042 output buffer full */
	if ((Stat & 0x01) != 0)
	{
	    Data = in(KEYB_PORT);

	    /* loop if parity error or receive timeout */
	    if((Stat & 0xC0) == 0)
		return Data;
	}
    }

    return -1;
}

uint8_t kbdWriteRead(uint16_t port, uint8_t data, const char* expect)
{
    int RetVal;

    kbdWrite(port, data);
    for (; *expect; expect++)
    {
	RetVal = kbdRead();
	if ((uint8_t) *expect != RetVal)
	{
	    TRACE2("[mouse] error: expected 0x%x, got 0x%x\n",
		*expect, RetVal);
	    return RetVal;
	}
    }

    return 0;
}

void ps2StartIo(Ps2Mouse* ctx)
{
    int but, dx, dy, dw;
    asyncio_t *io, *next;
    mouse_packet_t* pkt;

    /* Extract the data from the bytes read.
       From svgalib ms.c. */
    but = (ctx->data[0] & 0x04) >> 1 |    /* Middle */
	(ctx->data[0] & 0x02) >> 1 |  /* Right */
	(ctx->data[0] & 0x01) << 2;   /* Left */
    dx = (ctx->data[0] & 0x10) ? ctx->data[1] - 256 : ctx->data[1];
    dy = (ctx->data[0] & 0x20) ? -(ctx->data[2] - 256) : -ctx->data[2];
    dw = (int) ((signed char) ctx->data[3]);

    if (dx > 5 || dx < -5)
	dx *= 4;
    if (dy > 5 || dy < -5)
	dy *= 4;

    TRACE4("%d %d %x %d\t", dx, dy, but, dw);
    /*putwchar('!');*/
    
    for (io = ctx->dev.io_first; io != NULL; io = next)
    {
	/*pkt = (mouse_packet_t*) ((uint8_t*) DevMapBuffer(io) + io->mod_buffer_start + io->length);*/
        pkt = (mouse_packet_t*) ((uint8_t*) DevMapBuffer(io) + io->length);
	
	pkt->dx = dx;
	pkt->dy = dy;
	pkt->buttons = but;
	pkt->dwheel = dw;

	next = io->next;
	DevUnmapBuffer();

	io->length += sizeof(*pkt);
	if (io->length >= ((request_dev_t*) io->req)->params.dev_read.length)
	    DevFinishIo(&ctx->dev, io, 0);
	/*putwchar('.');*/
    }

    /*putwchar('\n');*/
}

bool ps2Isr(device_t *dev, uint8_t irq)
{
    if ((in(KEYB_CTRL) & 0x01) != 0)
    {
	Ps2Mouse *mouse;
        mouse = (Ps2Mouse*) dev;
        mouse->data[mouse->bytes++] = in(KEYB_PORT);
	if (mouse->bytes >= 3 + mouse->has_wheel)
	{
	    ps2StartIo(mouse);
	    mouse->bytes = 0;
	}
	
	return true;
    }

    return false;
}

bool ps2Request(device_t* dev, request_t* req)
{
    request_dev_t *req_dev;
    asyncio_t *io;

    req_dev = (request_dev_t*) req;
    TRACE2("{%c%c}", req->code / 256, req->code % 256);
    switch (req->code)
    {
    case DEV_READ:
	if (req_dev->params.dev_read.length % sizeof(mouse_packet_t))
	{
	    wprintf(L"%d: wrong size\n", req_dev->params.dev_read.length);
	    req->result = EBUFFER;
	    return false;
	}

	io = DevQueueRequest(dev, &req_dev->header, sizeof(*req_dev),
	    req_dev->params.dev_read.pages,
	    req_dev->params.dev_read.length);
	if (io == NULL)
	{
	    req->result = errno;
	    return false;
	}
	else
	{
	    io->length = 0;
	    return true;
	}
	    
    case DEV_REMOVE:
	/* xxx - shut down mouse */
	/*DevRegisterIrq(12, dev, false);*/
	free(dev);
	return true;
    }

    req->result = ENOTIMPL;
    return false;
}

static const device_vtbl_t ps2_vtbl =
{
    ps2Request,	/* request */
    ps2Isr,	/* isr */
    NULL,	/* finishio */
};

void msleep(unsigned ms)
{
    unsigned end;
    end = SysUpTime() + ms;
    while (SysUpTime() <= end)
        ;
}

void ps2AddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
    /* These strings nicked from gpm (I_imps2): I don't know how they work... */
    static uint8_t s1[] = { 0xF3, 0xC8, 0xF3, 0x64, 0xF3, 0x50, 0 };
    static uint8_t s2[] = { 0xF6, 0xE6, 0xF4, 0xF3, 0x64, 0xE8, 0x03, 0 };
    Ps2Mouse* ctx;
    const uint8_t* ch;
    uint8_t id;

    if (cfg->device_class == 0)
        cfg->device_class = 0x0902;

    ctx = malloc(sizeof(Ps2Mouse));
    memset(ctx, 0, sizeof(Ps2Mouse));
    ctx->dev.driver = drv;
    ctx->dev.vtbl = &ps2_vtbl;
    
    /* enable the aux port */
    kbdWrite(KEYB_CTRL, KCTRL_ENABLE_AUX);

    TRACE0("[mouse] String 1\n");
    for (ch = s1; *ch; ch++)
    {
        kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
        kbdWriteRead(KEYB_PORT, *ch, KEYB_ACK);
    }

    /* Bochs doesn't like this bit... */
    TRACE0("[mouse] String 2\n");
    for (ch = s2; *ch; ch++)
    {
        kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
        kbdWriteRead(KEYB_PORT, *ch, KEYB_ACK);
    }

    msleep(10);

    /* Identify mouse -- regular PS/2 mice should return zero here.
       Unfortunately, my Intellimouse PS/2 also returns zero unless it has
       been given the string 's2' above. Bochs doesn't support wheeled mice
       and panics when it receives the F6h above. Fix needed. */
    TRACE0("[mouse] Identify\n");
    kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
    kbdWriteRead(KEYB_PORT, AUX_IDENTIFY, KEYB_ACK);
    id = kbdRead();

    TRACE1("[mouse] Detected device type %x\n", id);
    ctx->has_wheel = id == 3;
    ctx->bytes = 0;
    memset(ctx->data, 0, sizeof(ctx->data));

    /*kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
    kbdWriteRead(KEYB_PORT, 0xF3, KEYB_ACK);
    kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
    kbdWriteRead(KEYB_PORT, 0xFF, KEYB_ACK);*/

    TRACE0("[mouse] Information\n");
    kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
    kbdWriteRead(KEYB_PORT, AUX_INFORMATION, KEYB_ACK);
    TRACE1("[mouse] status = %d\n", kbdRead());
    TRACE1("[mouse] resolution = %d\n", kbdRead());
    TRACE1("[mouse] sample rate = %d\n", kbdRead());

    /* enable aux device (mouse) */
    kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
    kbdWriteRead(KEYB_PORT, AUX_ENABLE, KEYB_ACK);

    DevRegisterIrq(12, &ctx->dev);
    
    wprintf(L"PS/2 mouse driver installed\n");
    DevAddDevice(&ctx->dev, name, cfg);
}

bool DrvInit(driver_t* drv)
{
    drv->add_device = ps2AddDevice;
    return true;
}

/*@}*/
