#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/handle.h>
#include <kernel/arch.h>

#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/*#define DEBUG */
#include <kernel/debug.h>

#include <kernel/driver.h>
#include "keyboard.h"
#include "british.h"

/* in tty.drv */
void TtySwitchConsoles(unsigned n);

/*!
 *  \ingroup	drivers
 *  \defgroup	keyboard    AT/PS2 keyboard
 *  @{
 */

typedef struct Keyboard Keyboard;
struct Keyboard
{
	device_t dev;
	uint32_t keys;
	wchar_t compose;
	uint32_t *write, *read;
	uint32_t buffer[128];
	bool isps2;
	uint16_t port, ctrl;
};

#define SET(var, bit, val) \
	{ \
		if (val) \
			var |= bit; \
		else \
			var &= ~bit; \
	}

void kbdHwWrite(Keyboard* ctx, uint16_t port, uint8_t data)
{
	uint32_t timeout;
	uint8_t stat;

	for (timeout = 500000L; timeout != 0; timeout--)
	{
		stat = in(ctx->ctrl);

		if ((stat & 0x02) == 0)
			break;
	}

	if (timeout != 0)
		out(port, data);
}

uint8_t kbdHwRead(Keyboard* ctx)
{
	unsigned long Timeout;
	uint8_t Stat, Data;

	for (Timeout = 50000L; Timeout != 0; Timeout--)
	{
		Stat = in(ctx->ctrl);

		/* loop until 8042 output buffer full */
		if ((Stat & 0x01) != 0)
		{
			Data = in(ctx->port);

			/* loop if parity error or receive timeout */
			if((Stat & 0xC0) == 0)
				return Data;
		}
	}

	return -1;
}

uint8_t kbdHwWriteRead(Keyboard* ctx, uint16_t port, uint8_t data, const char* expect)
{
	int RetVal;

	kbdHwWrite(ctx, port, data);
	for (; *expect; expect++)
	{
		RetVal = kbdHwRead(ctx);
		if ((uint8_t) *expect != RetVal)
		{
			TRACE2("[keyboard] error: expected 0x%x, got 0x%x\n",
				(uint8_t) *expect, RetVal);
			return RetVal;
		}
	}

	return 0;
}

uint32_t kbdScancodeToKey(Keyboard* ctx, uint8_t scancode)
{
	static int keypad[RAW_NUM0 - RAW_NUM7 + 1] =
	{
		7, 8, 9, -1,
		4, 5, 6, -1,
		1, 2, 3,
		0
	};

	bool down = (scancode & 0x80) == 0;
	uint8_t code = scancode & ~0x80;
	uint32_t key = 0;
	uint8_t temp;
	
	switch (code)
	{
	case RAW_LEFT_CTRL:
	case RAW_RIGHT_CTRL:
		SET(ctx->keys, KBD_BUCKY_CTRL, down);
		break;

	case RAW_LEFT_ALT:
	/*case RAW_RIGHT_ALT: */
		if (down && (ctx->keys & KBD_BUCKY_ALT) == 0)
			ctx->compose = 0;
		else if (!down && (ctx->keys & KBD_BUCKY_ALT))
			key = ctx->compose;

		SET(ctx->keys, KBD_BUCKY_ALT, down);
		break;

	case RAW_LEFT_SHIFT:
	case RAW_RIGHT_SHIFT:
		SET(ctx->keys, KBD_BUCKY_SHIFT, down);
		break;

	case RAW_NUM_LOCK:
		if (!down)
		{
			ctx->keys ^= KBD_BUCKY_NUM;
			goto leds;
		}
		break;

	case RAW_CAPS_LOCK:
		if (!down)
		{
			ctx->keys ^= KBD_BUCKY_CAPS;
			goto leds;
		}
		break;

	case RAW_SCROLL_LOCK:
		if (!down)
		{
			ctx->keys ^= KBD_BUCKY_SCRL;
			goto leds;
		}
		break;

leds:
		kbdHwWrite(ctx, ctx->port, KEYB_SET_LEDS);
		temp = 0;
		if (ctx->keys & KBD_BUCKY_SCRL)
			temp |= 1;
		if (ctx->keys & KBD_BUCKY_NUM)
			temp |= 2;
		if (ctx->keys & KBD_BUCKY_CAPS)
			temp |= 4;
		kbdHwWrite(ctx, ctx->port, temp);
		break;

	default:
		if (down)
		{
			if (code >= RAW_NUM7 && 
				code <= RAW_NUM0 &&
				code != 0x4a &&
				code != 0x4e)
			{
				if (ctx->keys & KBD_BUCKY_ALT &&
					keypad[code - RAW_NUM7] != -1)
				{
					ctx->compose *= 10;
					ctx->compose += keypad[code - RAW_NUM7];
					/*wprintf(L"pad = %d compose = %d\n",  */
						/*keypad[code - RAW_NUM7], ctx->compose); */
				}
				else if (ctx->keys & KBD_BUCKY_NUM)
					key = '0' + keypad[code - RAW_NUM7];
				else
					key = keys[code];
			}
			else if (ctx->keys & KBD_BUCKY_SHIFT)
				key = keys_shift[code];
			else
				key = keys[code];

			if (ctx->keys & KBD_BUCKY_CAPS)
			{
				if (iswupper(key))
					key = towlower(key);
				else if (iswlower(key))
					key = towupper(key);
			}
		}
	}
	
	return key | ctx->keys;
}

size_t kbdRead(Keyboard* ctx, void* buffer, size_t length)
{
	uint32_t ch, *buf = (uint32_t*) buffer;
	size_t read = 0;

	while (length > 0)
	{
		if (ctx->read == ctx->write)
			return read;
		
		ch = *ctx->read;
		ctx->read++;
		if (ctx->read >= ctx->buffer + _countof(ctx->buffer))
			ctx->read = ctx->buffer;

		*buf = ch;
		buf++;
		length -= sizeof(uint32_t);
		read += sizeof(uint32_t);
	}

	return read;
}

void kbdDoRequests(Keyboard* ctx)
{
	asyncio_t *io, *ionext;
	request_dev_t *req;
	size_t read;
	uint32_t key;

	while (ctx->dev.io_first &&
		(read = kbdRead(ctx, &key, sizeof(key))))
	{
		for (io = ctx->dev.io_first; io; io = ionext)
		{
			req = (request_dev_t*) io->req;
		
			wprintf(L"read = %d req = %p ", read, req);
			wprintf(L"buffer = %p size = %d\n", 
				io->buffer, 
				io->length);

			wprintf(L"Writing buffer...");

			*(uint32_t*) ((uint8_t*) io->buffer + io->length) = key;
			wprintf(L"done\n");
			io->length += read;
			
			ionext = io->next;
			if (io->length >= req->params.dev_read.length)
				DevFinishIo(&ctx->dev, io);
			/*else */
				/*_cputws(L"more to do\n"); */
		}
	}
}

void kbdKey(Keyboard* ctx, uint8_t scancode)
{
	uint32_t key;

	if (ctx->write >= ctx->read)
	{
		key = kbdScancodeToKey(ctx, scancode);

		/*if (key == (KBD_BUCKY_CTRL | KBD_BUCKY_ALT | KEY_DEL))
			keShutdown(SHUTDOWN_REBOOT);
		else if (key == KEY_F11)
			if (dbgInvoke(thrCurrent(), thrContext(thrCurrent()), 0))
				return;*/
		
		if (key >= KEY_F1 && key <= KEY_F12)
			TtySwitchConsoles(key - KEY_F1);
		else if (key)
		{
			*ctx->write = key;
			ctx->write++;

			if (ctx->write >= ctx->buffer + _countof(ctx->buffer))
				ctx->write = ctx->buffer;

			/*wprintf(L"read = %d, write = %d\n", ctx->read - ctx->buffer, ctx->write - ctx->buffer); */
			/*Read(&key, sizeof(key)); */
			/*wprintf(L"%c", key); */
		}

		kbdDoRequests(ctx);
	}
}

bool KbdIsr(device_t *dev, uint8_t irq)
{
	Keyboard* ctx = (Keyboard*) dev;
	kbdKey(ctx, kbdHwRead(ctx));
	return true;
}

void KbdFinishIo(device_t *dev, asyncio_t *io)
{
	request_dev_t *req_dev = (request_dev_t*) io->req;
	memcpy(req_dev->params.dev_read.buffer,
		io->buffer,
		io->length);
	free(io->buffer);
}

bool kbdRequest(device_t* dev, request_t* req)
{
	Keyboard* ctx = (Keyboard*) dev;
	request_dev_t *req_dev = (request_dev_t*) req;

	TRACE2("[%c%c]", req->code / 256, req->code % 256);
	switch (req->code)
	{
	case DEV_READ:
		DevQueueRequest(dev, req, sizeof(request_dev_t));
		req_dev->header.user_length = req_dev->params.dev_read.length;
		req_dev->params.dev_read.length = 0;
		kbdDoRequests(ctx);
		return true;

	case CHR_GETSIZE:
		*((size_t*) req_dev->params.buffered.buffer) = ctx->write - ctx->read;
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

Keyboard* kbdInit(driver_t* drv, device_config_t *cfg)
{
	Keyboard* ctx;
	uint32_t i;
	uint16_t port, ctrl;
	device_t* kdebug;
	uint8_t status;

	ctx = (Keyboard*) malloc(sizeof(Keyboard));

	/*if (cfg)
		i = devFindResource(cfg, dresIo, 0);
	else
		i = -1;

	if (i == (uint32_t) -1)
		port = KEYB_PORT;
	else
		port = cfg->resources[i].u.io.base;

	if (cfg)
		i = devFindResource(cfg, dresIo, 1);
	else
		i = -1;

	if (i == (uint32_t) -1)
		ctrl = KEYB_CTRL;
	else
		ctrl = cfg->resources[i].u.io.base;*/

	port = KEYB_PORT;
	ctrl = KEYB_CTRL;

	ctx->dev.driver = drv;
	ctx->dev.request = kbdRequest;
	ctx->dev.cfg = cfg;
	ctx->dev.req_first = ctx->dev.req_last = NULL;
	ctx->keys = 0;
	ctx->write = ctx->read = ctx->buffer;
	ctx->port = port;
	ctx->ctrl = ctrl;

	TRACE2("keyboard: port = %x ctrl = %x\n", port, ctrl);

	/*
	 * The kernel debugger may have registered our IRQ1, which would make a 
	 *	mess of keyboard initialization.
	 */
	/*kdebug = devOpen(L"debugger", NULL);
	devRegisterIrq(kdebug, 1, false);
	devClose(kdebug);*/

#if 0
	out(port, 0xff); /*reset keyboard   */ 
    do {
        status = in(ctrl);
    } while (status & 0x02);

    out(ctrl, 0xAA); /*selftest   */
    in(port);
    out(ctrl, 0xAB); /*interface selftest   */
    in(port);

    out(ctrl, 0xAE); /*enable keyboard   */

    out(port, 0xff); /*reset keyboard   */

    in(port);
    in(port);

    out(ctrl, 0xAD); /*disable keyboard   */

    out(ctrl, 0x60);
    out(port, 0x01 | 0x04 | 0x20 | 0x40);

    out(ctrl, 0xAE); /*enable keyboard   */
#else
	/* Reset keyboard and disable scanning until further down */
	TRACE0("Disable...");
	kbdHwWriteRead(ctx, ctx->port, KEYB_RESET_DISABLE, KEYB_ACK);

	/* Set keyboard controller flags: 
	   interrupts for keyboard & aux port
	   SYS bit (?) */
	TRACE0("done\nFlags...");
	kbdHwWrite(ctx, ctx->ctrl, KCTRL_WRITE_CMD_BYTE);
	kbdHwWrite(ctx, ctx->port, KCTRL_IRQ1 | KCTRL_SYS | KCTRL_IRQ12);

	TRACE0("done\nIdentify...");
	kbdHwWriteRead(ctx, ctx->port, KEYB_IDENTIFY, KEYB_ACK);
	if (kbdHwRead(ctx) == 0xAB &&
		kbdHwRead(ctx) == 0x83)
	{
		TRACE0("[keyboard] PS/2 keyboard detected\n");

		/* Program desired scancode set, expect 0xFA (ACK)... */
		kbdHwWriteRead(ctx, ctx->port, KEYB_SET_SCANCODE_SET, KEYB_ACK);

		/* ...send scancode set value, expect 0xFA (ACK) */
		kbdHwWriteRead(ctx, ctx->port, 1, KEYB_ACK);

		/* make all keys typematic (auto-repeat) and make-break. This may work
		   only with scancode set 3, I'm not sure. Expect 0xFA (ACK) */
		kbdHwWriteRead(ctx, ctx->port, KEYB_ALL_TYPM_MAKE_BRK, KEYB_ACK);

		ctx->isps2 = true;
	}
	else
	{
		/* Argh... bloody bochs */
		TRACE0("[keyboard] AT keyboard detected\n");
		ctx->isps2 = false;
	}

	/* Set typematic delay as short as possible;
	   Repeat as fast as possible, expect ACK... */
	TRACE0("Typematic...");
	kbdHwWriteRead(ctx, ctx->port, KEYB_SET_TYPEMATIC, KEYB_ACK);

	/* ...typematic control uint8_t (0 corresponds to MODE CON RATE=30 DELAY=1),
	   expect 0xFA (ACK) */
	kbdHwWriteRead(ctx, ctx->port, 0, KEYB_ACK);

	/* Enable keyboard, expect 0xFA (ACK) */
	TRACE0("done\nEnable...");
	kbdHwWriteRead(ctx, ctx->port, KEYB_ENABLE, KEYB_ACK);
#endif

	TRACE0("done\nIRQ...");
	DevRegisterIrq(1, &ctx->dev);
	TRACE0("done\n");
	return ctx;
}

device_t* kbdAddDevice(driver_t* drv, const wchar_t* name, 
					   device_config_t* cfg)
{
	return (device_t*) kbdInit(drv, cfg);
}

bool DrvInit(driver_t* drv)
{
	drv->add_device = kbdAddDevice;
	/*devRegister(L"keyboard", kbdAddDevice(drv, L"keyboard", NULL), NULL); */
	return true;
}

/*@} */