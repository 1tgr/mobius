/*****************************************************************************
serial mouse driver
*****************************************************************************/
#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/sys.h>
#include <os/mouse.h>
#include <errno.h>

#define DEBUG
#include <kernel/debug.h>

//#define peek40(OFF) *((word*) (0x400 + (OFF))) // xxx - FIX!
#define	BUF_SIZE	64

typedef struct	/* circular queue */
{
	unsigned char *data;
	unsigned size, in_ptr, out_ptr;
} queue_t;

typedef struct
{
	device_t dev;
	unsigned char _irq, _buffer[BUF_SIZE];
	unsigned short _io_adr;
	queue_t _queue;
	/* which COM port to use (1=COM1, 2=COM2, etc.) */
	unsigned char _com;
} SerMouse;

/*****************************************************************************
*****************************************************************************/
void enable_irq_at_8259(unsigned short irq)
{
	unsigned char mask;

	if(irq < 8)
	{
		mask = 1 << irq;
		out(0x21, in(0x21) & ~mask);
	}
	else if(irq < 16)
	{
		irq -= 8;
		mask = 1 << irq;
		out(0xA1, in(0xA1) & ~mask);
		out(0x21, in(0x21) & ~0x04);
	}
}
/*****************************************************************************
*****************************************************************************/
void disable_irq_at_8259(unsigned short irq)
{
	unsigned char mask;

	if(irq < 8)
	{
		mask = 1 << irq;
		out(0x21, in(0x21) | mask);
	}
	else if(irq < 16)
	{
		irq -= 8;
		mask = 1 << irq;
		out(0xA1, in(0xA1) | mask);
	}
}

static __inline__ unsigned crit_begin(void)
{
	unsigned ret_val;

	__asm__ __volatile__("pushfl\n"
		"popl %0\n"
		"cli"
		: "=a"(ret_val)
		:);
	return ret_val;
}

static __inline__ void crit_end(unsigned flags)
{
	__asm__ __volatile__("pushl %0\n"
		"popfl"
		:
		: "m"(flags)
		);
}

int inq(queue_t *q, unsigned char data)
{
	unsigned flags, temp;

	flags = crit_begin();
	temp = q->in_ptr + 1;
	if(temp >= q->size)
		temp = 0;
/* if in_ptr reaches out_ptr, the queue is full */
	if(temp == q->out_ptr)
	{
		crit_end(flags);
		return -1;
	}
	q->data[q->in_ptr] = data;
	q->in_ptr = temp;
	crit_end(flags);
	return 0;
}
/*****************************************************************************
*****************************************************************************/
int deq(queue_t *q, unsigned char *data)
{
	unsigned flags;

	flags = crit_begin();
/* if in_ptr == out_ptr, the queue is empty */
	if(q->in_ptr == q->out_ptr)
	{
		crit_end(flags);
		return -1;
	}
	*data = q->data[q->out_ptr];
	q->out_ptr++;
	if(q->out_ptr >= q->size)
		q->out_ptr = 0;
	crit_end(flags);
	return 0;
}

bool mseRequest(device_t* dev, request_t* req)
{
	unsigned char temp;
	SerMouse *ctx = (SerMouse*) dev;

	TRACE2("{%c%c}", req->code / 256, req->code % 256);
	switch (req->code)
	{
	case DEV_READ:
		if (req->params.read.length < sizeof(mouse_packet_t))
		{
			TRACE1("%d: too small\n", req->params.read.length);
			req->result = EBUFFER;
			return false;
		}

		devStartRequest(dev, req);
		return true;
		
	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;

	case DEV_REMOVE:
		/* disable interrupts at serial chip */
		out(ctx->_io_adr + 1, 0);
		/* disable interrupts at 8259s */
		disable_irq_at_8259(ctx->_irq);
		hndFree(ctx);
		hndSignal(req->event, true);
		return true;

	case DEV_ISR:
		temp = in(ctx->_io_adr  + 5);
		if (temp & 0x01)
		{
			temp = in(ctx->_io_adr + 0);
			inq(&ctx->_queue, temp);
		}

		in(ctx->_io_adr + 2);

		// req->event ignored here
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

/*****************************************************************************
*****************************************************************************/
#define	MAX_ID	16

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	static const unsigned char com_to_irq[] =
	{
		4, 3, 4, 3
	};

	static const word io_port[] =
	{
		0x3f8, 0x2f8, 0x3f8, 0x2f8
	};

/**/
	unsigned char b, id[MAX_ID], *ptr;
	unsigned temp;
	SerMouse *ctx;

	ctx = hndAlloc(sizeof(SerMouse), NULL);
	
	ctx->dev.driver = drv;
	ctx->dev.request = mseRequest;
	ctx->dev.req_first = ctx->dev.req_last = NULL;
	ctx->_queue.data = ctx->_buffer;
	ctx->_queue.size = BUF_SIZE;
	ctx->_queue.in_ptr = ctx->_queue.out_ptr = 0;
	ctx->_com = 1;

	if (ctx->_com < 1 || ctx->_com > 4)
	{
		TRACE1("Invalid COM port %u (must be 1-4)\n", ctx->_com);
		return false;
	}
/* get COM port I/O address from BIOS data segment */
	//ctx->_io_adr = peek40(0 + (ctx->_com - 1) * 2);
	ctx->_io_adr = io_port[ctx->_com - 1];
	if(ctx->_io_adr == 0)
	{
		TRACE1("Sorry, no COM%u serial port on this PC\n", ctx->_com);
		return false;
	}
	TRACE1("activating mouse on COM%u...\n", ctx->_com);
	ctx->_irq = com_to_irq[ctx->_com - 1];
	/* install handler */
	devRegisterIrq((device_t*) ctx, ctx->_irq, true);

	
/* set up serial chip
turn off handshaking lines to power-down mouse, then delay */
	out(ctx->_io_adr + 4, 0);
	msleep(500);
	out(ctx->_io_adr + 3, 0x80);	/* set DLAB to access baud divisor */
	out(ctx->_io_adr + 1, 0);	/* 1200 baud */
	out(ctx->_io_adr + 0, 96);
	out(ctx->_io_adr + 3, 0x02);	/* clear DLAB bit, set 7N1 format */
	out(ctx->_io_adr + 2, 0);	/* turn off FIFO, if any */
/* activate Out2, RTS, and DTR to power the mouse */
	out(ctx->_io_adr + 4, 0x0B);
/* enable receiver interrupts */
	out(ctx->_io_adr + 1, 0x01);
/* enable interrupts at 8259s */
	enable_irq_at_8259(ctx->_irq);
/* wait a moment to get ID bytes from mouse. Microsoft mice just return
a single 'M' byte, some 3-button mice return "M3", my Logitech mouse
returns a string of bytes. I could find no documentation on these,
but I figured out some of it on my own. */
	ptr = id;
	for(temp = 750; temp != 0; temp--)
	{
		while(deq(&ctx->_queue, &b) == 0)
		{
			TRACE1("%02X ", b);
			*ptr = b;
			ptr++;
			if(ptr >= id + MAX_ID)
				goto FOO;
		}
		msleep(1);
	}
	TRACE0("\n");

	if (id == ptr)
		TRACE0("No serial mouse found\n");

FOO:
	if(ptr >= id + 11)
	{
/* find the 'M' */
		for(ptr = id; ptr < id + MAX_ID - 7; ptr++)
		{
			if(*ptr == 'M')
				break;
		}
		if(ptr < id + MAX_ID)
		{
/* four bytes that each encode 4 bits of the numeric portion
of the PnP ID. Each byte has b4 set (i.e. ORed with 0x10) */
			temp = (ptr[8] & 0x0F);
			temp <<= 4;
			temp |= (ptr[9] & 0x0F);
			temp <<= 4;
			temp |= (ptr[10] & 0x0F);
			temp <<= 4;
			temp |= (ptr[11] & 0x0F);
/* three bytes that each encode one character of the character portion
of the PnP ID. Each byte has b5 set (i.e. ORed with 0x20) */
			TRACE4("mouse PnP ID: %c%c%c%04X\n",
				'@' + (ptr[5] & 0x1F),
				'@' + (ptr[6] & 0x1F),
				'@' + (ptr[7] & 0x1F), temp);
		}
/* example: Logitech serial mouse LGI8001
('L' - '@') | 0x20 == 0x2C
('G' - '@') | 0x20 == 0x27
('I' - '@') | 0x20 == 0x29
('8' - '0') | 0x10 == 0x18
('0' - '0') | 0x10 == 0x10
('0' - '0') | 0x10 == 0x10
('1' - '0') | 0x10 == 0x11 */
	}
	

	return true;
}
