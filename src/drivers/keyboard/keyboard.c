#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <kernel/kernel.h>
#include <kernel/sys.h>
#include <kernel/thread.h>
#include <kernel/handle.h>
#include <errno.h>

#include <kernel/debug.h>

#include <kernel/driver.h>
#include "keyboard.h"
#include "british.h"

typedef struct Keyboard Keyboard;
struct Keyboard
{
	device_t dev;
	dword keys;
	wchar_t compose;
	dword *write, *read;
	dword buffer[128];
	bool isps2;
	word port, ctrl;
};

#define SET(var, bit, val) \
	{ \
		if (val) \
			var |= bit; \
		else \
			var &= ~bit; \
	}

void kbdHwWrite(Keyboard* ctx, word port, byte data)
{
	dword timeout;
	byte stat;

	for (timeout = 500000L; timeout != 0; timeout--)
	{
		stat = in(ctx->ctrl);

		if ((stat & 0x02) == 0)
			break;
	}

	if (timeout != 0)
		out(port, data);
}

byte kbdHwRead(Keyboard* ctx)
{
	unsigned long Timeout;
	byte Stat, Data;

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

byte kbdHwWriteRead(Keyboard* ctx, word port, byte data, const char* expect)
{
	int RetVal;

	kbdHwWrite(ctx, port, data);
	for (; *expect; expect++)
	{
		RetVal = kbdHwRead(ctx);
		if ((byte) *expect != RetVal)
		{
			TRACE2("[keyboard] error: expected 0x%x, got 0x%x\n",
				(byte) *expect, RetVal);
			return RetVal;
		}
	}

	return 0;
}

dword kbdScancodeToKey(Keyboard* ctx, byte scancode)
{
	static int keypad[RAW_NUM0 - RAW_NUM7 + 1] =
	{
		7, 8, 9, -1,
		4, 5, 6, -1,
		1, 2, 3,
		0
	};

	bool down = (scancode & 0x80) == 0;
	byte code = scancode & ~0x80;
	dword key = 0;
	byte temp;
	
	switch (code)
	{
	case RAW_LEFT_CTRL:
	case RAW_RIGHT_CTRL:
		SET(ctx->keys, KBD_BUCKY_CTRL, down);
		break;

	case RAW_LEFT_ALT:
	//case RAW_RIGHT_ALT:
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
					//wprintf(L"pad = %d compose = %d\n", 
						//keypad[code - RAW_NUM7], ctx->compose);
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
	dword ch, *buf = (dword*) buffer;
	size_t read = 0;

	while (length > 0)
	{
		if (ctx->read == ctx->write)
			return read;
		
		ch = *ctx->read;
		ctx->read++;
		if (ctx->read >= ctx->buffer + countof(ctx->buffer))
			ctx->read = ctx->buffer;

		*buf = ch;
		buf++;
		length -= sizeof(dword);
		read += sizeof(dword);
	}

	return read;
}

void kbdDoRequests(Keyboard* ctx)
{
	request_t *req, *next;
	size_t read;
	dword key;

	while (ctx->dev.req_first &&
		(read = kbdRead(ctx, &key, sizeof(key))))
	{
		for (req = ctx->dev.req_first; req; req = next)
		{
			/*wprintf(L"read = %d req = %p ", read, req);
			wprintf(L"buffer = %p size = %d\n", req->params.read.buffer, 
				req->params.read.length);

			_cputws(L"Writing buffer...");*/
			((dword*) req->params.read.buffer)[req->params.read.length / sizeof(dword)] = key;
			//_cputws(L"done\nUpdating length...");
			req->params.read.length += read;
			
			next = req->next;
			if (req->params.read.length >= req->user_length)
			{
				//_cputws(L"finished\n");
				devFinishRequest(&ctx->dev, req);
			}
			//else
				//_cputws(L"more to do\n");
		}
	}
}

void kbdKey(Keyboard* ctx, byte scancode)
{
	dword key;

	if (ctx->write >= ctx->read)
	{
		key = kbdScancodeToKey(ctx, scancode);

		if (key == (KBD_BUCKY_CTRL | KBD_BUCKY_ALT | KEY_DEL))
			keShutdown(SHUTDOWN_REBOOT);
		else if (key == KEY_F11)
			if (dbgInvoke(thrCurrent(), thrContext(thrCurrent()), 0))
				return;
		
		if (key)
		{
			*ctx->write = key;
			ctx->write++;

			if (ctx->write >= ctx->buffer + countof(ctx->buffer))
				ctx->write = ctx->buffer;

			//wprintf(L"read = %d, write = %d\n", ctx->read - ctx->buffer, ctx->write - ctx->buffer);
			//Read(&key, sizeof(key));
			//wprintf(L"%c", key);
		}

		kbdDoRequests(ctx);
	}
}

bool kbdRequest(device_t* dev, request_t* req)
{
	Keyboard* ctx = (Keyboard*) dev;

	TRACE2("[%c%c]", req->code / 256, req->code % 256);
	switch (req->code)
	{
	case DEV_READ:
		devStartRequest(dev, req);
		req->params.read.length = 0;

		kbdDoRequests(ctx);
		return true;

	case DEV_OPEN:
	case DEV_CLOSE:
		hndSignal(req->event, true);
		return true;

	case DEV_ISR:
		kbdKey(ctx, kbdHwRead(ctx));
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

Keyboard* INIT_CODE kbdInit(driver_t* drv, device_config_t *cfg)
{
	Keyboard* ctx;
	dword i;
	word port, ctrl;
	device_t* kdebug;

	ctx = (Keyboard*) hndAlloc(sizeof(Keyboard), NULL);

	if (cfg)
		i = devFindResource(cfg, dresIo, 0);
	else
		i = -1;

	if (i == (dword) -1)
		port = KEYB_PORT;
	else
		port = cfg->resources[i].u.io.base;

	if (cfg)
		i = devFindResource(cfg, dresIo, 1);
	else
		i = -1;

	if (i == (dword) -1)
		ctrl = KEYB_CTRL;
	else
		ctrl = cfg->resources[i].u.io.base;

	ctx->dev.driver = drv;
	ctx->dev.request = kbdRequest;
	ctx->dev.config = cfg;
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
	kdebug = devOpen(L"debugger", NULL);
	devRegisterIrq(kdebug, 1, false);
	devClose(kdebug);

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

	/* ...typematic control byte (0 corresponds to MODE CON RATE=30 DELAY=1),
	   expect 0xFA (ACK) */
	kbdHwWriteRead(ctx, ctx->port, 0, KEYB_ACK);

	/* Enable keyboard, expect 0xFA (ACK) */
	TRACE0("done\nEnable...");
	kbdHwWriteRead(ctx, ctx->port, KEYB_ENABLE, KEYB_ACK);

	TRACE0("done\nIRQ...");
	devRegisterIrq(&ctx->dev, 1, true);
	TRACE0("done\n");
	return ctx;
}

device_t* kbdAddDevice(driver_t* drv, const wchar_t* name, 
					   device_config_t* cfg)
{
	_cputws_check(L" \n" CHECK_L2 L"Keyboard\r\t");
	return (device_t*) kbdInit(drv, cfg);
}

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	drv->add_device = kbdAddDevice;
	//devRegister(L"keyboard", kbdAddDevice(drv, L"keyboard", NULL), NULL);
	return true;
}