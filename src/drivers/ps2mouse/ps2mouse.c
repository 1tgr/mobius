#include <stdlib.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/sys.h>
#include <errno.h>
#include <os/mouse.h>
#include <kernel/thread.h>

#define DEBUG
#include <kernel/debug.h>

/*!
 *  \ingroup	drivers
 *  \defgroup	ps2mouse    PS/2 aux port and mouse
 *  @{
 */

/*
 * General keyboard defines --
 *	needed for i8042 port (PS/2 controller)
 */

#define KEYB_PORT               0x60    /* keyboard port */
#define KEYB_CTRL               0x64    /* keyboard controller port */

#define KCTRL_ENABLE_AUX		0xA8	/* enable aux port (PS/2 mouse) */
#define KCTRL_WRITE_CMD_BYTE    0x60    /* write to command register */
#define KCTRL_WRITE_AUX			0xD4	/* write next byte at port 60 to aux port */

/* flags for KCTRL_WRITE_CMD_BYTE */
#define KCTRL_IRQ1              0x01
#define KCTRL_IRQ12             0x02
#define KCTRL_SYS               0x04
#define KCTRL_OVERRIDE_INHIBIT  0x08
#define KCTRL_DISABLE_KEYB      0x10
#define KCTRL_DISABLE_AUX       0x20
#define KCTRL_TRANSLATE_XT      0x40

/* commands to keyboard */
#define KEYB_SET_LEDS			0xED
#define KEYB_SET_SCANCODE_SET   0xF0
#define KEYB_IDENTIFY           0xF2
#define KEYB_SET_TYPEMATIC      0xF3
#define KEYB_ENABLE             0xF4
#define KEYB_RESET_DISABLE      0xF5
#define KEYB_ALL_TYPM_MAKE_BRK  0xFA

/* default ACK from keyboard following command */
#define KEYB_ACK                "\xFA"

/* commands to aux device (PS/2 mouse) */
#define AUX_INFORMATION			0xE9
#define AUX_ENABLE				0xF4
#define AUX_IDENTIFY			0xF2
#define AUX_RESET				0xFF

static word interrupts;

typedef struct Ps2Mouse Ps2Mouse;
struct Ps2Mouse
{
	device_t dev;
	bool has_wheel;
};

void INIT_CODE kbdWrite(word port, byte data)
{
	dword timeout;
	byte stat;

	for (timeout = 500000L; timeout != 0; timeout--)
	{
		stat = in(KEYB_CTRL);

		if ((stat & 0x02) == 0)
			break;
	}

	if (timeout != 0)
		out(port, data);
}

byte kbdRead()
{
	unsigned long Timeout;
	byte Stat, Data;

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

byte INIT_CODE kbdWriteRead(word port, byte data, const char* expect)
{
	int RetVal;

	kbdWrite(port, data);
	for (; *expect; expect++)
	{
		RetVal = kbdRead();
		if ((byte) *expect != RetVal)
		{
			TRACE2("[keyboard] error: expected 0x%x, got 0x%x\n",
				*expect, RetVal);
			return RetVal;
		}
	}

	return 0;
}

byte ps2AuxRead()
{
	byte Stat, Data;

	//enable();

	while (true)
	{
		//while ((interrupts & 0x1000) == 0)
			//;

		//interrupts &= ~0x1000;
		
		Stat = in(KEYB_CTRL);
		if ((Stat & 0x01) != 0)
		{
			Data = in(KEYB_PORT);

			/* loop if parity error or receive timeout */
			if((Stat & 0xC0) == 0)
				return Data;
		}
	}

	return (byte) -1;
}

void ps2Packet(Ps2Mouse* ctx)
{
	int but, dx, dy, dw;
	byte buf[4];
	request_t *req, *next;
	mouse_packet_t* pkt;
	
	//putwchar(L'[');
	buf[0] = ps2AuxRead();
	if (buf[0] == (byte) -1)
	{
		//putwchar(')');
		return;
	}

	buf[1] = ps2AuxRead();
	buf[2] = ps2AuxRead();

	if (ctx->has_wheel)
		buf[3] = ps2AuxRead();
	else
		buf[3] = 0;

	//putwchar(L']');
	//putwchar(L'\r');

	/* Extract the data from the bytes read.
	   From svgalib ms.c. */
	but = (buf[0] & 0x04) >> 1 |    /* Middle */
		(buf[0] & 0x02) >> 1 |  /* Right */
		(buf[0] & 0x01) << 2;   /* Left */
	dx = (buf[0] & 0x10) ? buf[1] - 256 : buf[1];
	dy = (buf[0] & 0x20) ? -(buf[2] - 256) : -buf[2];
	dw = (int) ((signed char) buf[3]);

	if (dx > 5 || dx < -5)
		dx *= 4;
	if (dy > 5 || dy < -5)
		dy *= 4;

	TRACE4("%d %d %x %d\t", dx, dy, but, dw);
	//putwchar('!');
	
	if (ctx->dev.req_first)
	{
		for (req = ctx->dev.req_first; req; req = next)
		{
			//wprintf(L"ps2mouse: req = %p ", req);
			pkt = (mouse_packet_t*) req->params.read.buffer;
			//wprintf(L"pkt = %p\n", pkt);

			pkt->dx = dx;
			pkt->dy = dy;
			pkt->buttons = but;
			pkt->dwheel = dw;

			next = req->next;
			devFinishRequest(&ctx->dev, req);
			//putwchar('.');
		}

		//putwchar('\n');
		//thrSchedule();
	}
}

bool ps2Request(device_t* dev, request_t* req)
{
	static int in_isr = 0;
	byte stat;

	TRACE2("{%c%c}", req->code / 256, req->code % 256);
	switch (req->code)
	{
	case DEV_READ:
		if (req->params.read.length < sizeof(mouse_packet_t))
		{
			wprintf(L"%d: too small\n", req->params.read.length);
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
		// xxx - shut down mouse
		devRegisterIrq(dev, 12, false);
		hndFree(dev);
		hndSignal(req->event, true);
		return true;

	case DEV_ISR:
		stat = in(KEYB_CTRL);
		if ((stat & 0x01) != 0)
		{
			interrupts |= 1 << req->params.isr.irq;
			if (!in_isr)
			{
				in_isr++;
				ps2Packet((Ps2Mouse*) dev);
				in_isr--;
			}
		}

		// req->event ignored here
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

Ps2Mouse* INIT_CODE ps2Init(driver_t* drv, device_config_t* cfg)
{
	/* These strings nicked from gpm (I_imps2): I don't know how they work... */
	static byte s1[] = { 0xF3, 0xC8, 0xF3, 0x64, 0xF3, 0x50, 0 };
	//static byte s2[] = { 0xF6, 0xE6, 0xF4, 0xF3, 0x64, 0xE8, 0x03, 0 };
	Ps2Mouse* ctx;
	const byte* ch;
	byte id;

	ctx = (Ps2Mouse*) hndAlloc(sizeof(Ps2Mouse), NULL);
	ctx->dev.driver = drv;
	ctx->dev.request = ps2Request;
	ctx->dev.req_first = ctx->dev.req_last = NULL;

	/* enable the aux port */
	kbdWrite(KEYB_CTRL, KCTRL_ENABLE_AUX);

	for (ch = s1; *ch; ch++)
	{
		kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
		kbdWriteRead(KEYB_PORT, *ch, KEYB_ACK);
	}

	/* Bochs doesn't like this bit... */
#if 0
	for (ch = s2; *ch; ch++)
	{
		kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
		kbdWriteRead(KEYB_PORT, *ch, KEYB_ACK);
	}
#endif

	msleep(10);

	/* Identify mouse -- regular PS/2 mice should return zero here.
	   Unfortunately, my Intellimouse PS/2 also returns zero unless it has
	   been given the string 's2' above. Bochs doesn't support wheeled mice
	   and panics when it receives the F6h above. Fix needed. */
	kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
	kbdWriteRead(KEYB_PORT, AUX_IDENTIFY, KEYB_ACK);
	id = kbdRead();

	ctx->has_wheel = id == 3;
	
	kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
	kbdWriteRead(KEYB_PORT, 0xF3, KEYB_ACK);
	kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
	kbdWriteRead(KEYB_PORT, 0xFF, KEYB_ACK);

	kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
	kbdWriteRead(KEYB_PORT, AUX_INFORMATION, KEYB_ACK);
	TRACE1("[mouse] status = %d\n", kbdRead());
	TRACE1("[mouse] resolution = %d\n", kbdRead());
	TRACE1("[mouse] sample rate = %d\n", kbdRead());

	/* enable aux device (mouse) */
	kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
	kbdWriteRead(KEYB_PORT, AUX_ENABLE, KEYB_ACK);

	devRegisterIrq((device_t*) ctx, 12, true);
	
	return ctx;
}

device_t* ps2AddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
	Ps2Mouse* mse = ps2Init(drv, cfg);
	_cputws(L"PS/2 mouse driver installed\n");
	return (device_t*) mse;
}

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	drv->add_device = ps2AddDevice;
	return true;
}

//@}