/* $Id: keyboard.c,v 1.2 2003/06/05 21:59:53 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/handle.h>
#include <kernel/arch.h>
#include <kernel/memory.h>
#include <kernel/profile.h>
#include <kernel/fs.h>

#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <os/ioctl.h>
#include <os/syscall.h>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <kernel/driver.h>
#include "keyboard.h"

keydef_t keys[94];

/* in tty.drv */
void TtySwitchConsoles(unsigned n);

/*!
 *  \ingroup	drivers
 *  \defgroup	 keyboard    AT/PS2 keyboard
 *  @{
 */

typedef struct keyboard_t keyboard_t;
struct keyboard_t
{
    device_t dev;
    uint32_t keys;
    wchar_t compose;
    uint32_t *write, *read;
    uint32_t buffer[64];
    bool isps2;
    bool extended;
    uint16_t port, ctrl;
};

#define SET(var, bit, val) \
    { \
	if (val) \
	    var |= bit; \
	else \
	    var &= ~bit; \
    }

void kbdHwWrite(keyboard_t* keyb, uint16_t port, uint8_t data)
{
    uint32_t timeout;
    uint8_t stat;

    for (timeout = 500000L; timeout != 0; timeout--)
    {
	stat = in(keyb->ctrl);

	if ((stat & 0x02) == 0)
	    break;
    }

    if (timeout != 0)
	out(port, data);
}

uint8_t kbdHwRead(keyboard_t* keyb)
{
    unsigned long Timeout;
    uint8_t Stat, Data;

    for (Timeout = 50000L; Timeout != 0; Timeout--)
    {
	Stat = in(keyb->ctrl);

	/* loop until 8042 output buffer full */
	if ((Stat & 0x01) != 0)
	{
	    Data = in(keyb->port);

	    /* loop if parity error or receive timeout */
	    if((Stat & 0xC0) == 0)
		return Data;
	}
    }

    return -1;
}

uint8_t kbdHwWriteRead(keyboard_t* keyb, uint16_t port, uint8_t data, const char* expect)
{
    int RetVal;

    kbdHwWrite(keyb, port, data);
    for (; *expect; expect++)
    {
	RetVal = kbdHwRead(keyb);
	if ((uint8_t) *expect != RetVal)
	{
	    TRACE2("[keyboard] error: expected 0x%x, got 0x%x\n",
		(uint8_t) *expect, RetVal);
	    return RetVal;
	}
    }

    return 0;
}

uint32_t KbdScancodeToKey(keyboard_t* keyb, uint8_t scancode)
{
    static int keypad[RAW_NUM0 - RAW_NUM7 + 1] =
    {
	7, 8, 9, -1,
	4, 5, 6, -1,
	1, 2, 3,
	0
    };

    bool down = (scancode & 0x80) == 0;
    uint16_t code = scancode & ~0x80;
    uint32_t key = 0;
    uint8_t temp;

    /*
     * xxx - I thought the extended scan code was 0xe0. HelpPC seems to think 
     *   so too.
     */
    if (code == 0x60)
    {
        keyb->extended = true;
        return 0;
    }
    else
    {
        if (keyb->extended)
            code |= 0x6000;
        keyb->extended = false;
    }

    switch (code)
    {
    case RAW_LEFT_CTRL:
    case RAW_RIGHT_CTRL:
	SET(keyb->keys, KBD_BUCKY_CTRL, down);
	break;

    case RAW_LEFT_ALT:
    	if (down && (keyb->keys & KBD_BUCKY_ALT) == 0)
	    keyb->compose = 0;
	else if (!down && (keyb->keys & KBD_BUCKY_ALT))
        {
            if (keyb->compose != 0)
                key = keyb->compose;
            else
                key = KBD_BUCKY_ALT | KBD_BUCKY_RELEASE;
        }

	SET(keyb->keys, KBD_BUCKY_ALT, down);
	break;

    case RAW_RIGHT_ALT:
        SET(keyb->keys, KBD_BUCKY_ALTGR, down);
        break;

    case RAW_LEFT_SHIFT:
    case RAW_RIGHT_SHIFT:
	SET(keyb->keys, KBD_BUCKY_SHIFT, down);
	break;

    case RAW_NUM_LOCK:
	if (!down)
	{
	    keyb->keys ^= KBD_BUCKY_NUM;
	    goto leds;
	}
	break;

    case RAW_CAPS_LOCK:
	if (!down)
	{
	    keyb->keys ^= KBD_BUCKY_CAPS;
	    goto leds;
	}
	break;

    case RAW_SCROLL_LOCK:
	if (!down)
	{
	    keyb->keys ^= KBD_BUCKY_SCRL;
	    goto leds;
	}
	break;

leds:
	kbdHwWrite(keyb, keyb->port, KEYB_SET_LEDS);
	temp = 0;
	if (keyb->keys & KBD_BUCKY_SCRL)
	    temp |= 1;
	if (keyb->keys & KBD_BUCKY_NUM)
	    temp |= 2;
	if (keyb->keys & KBD_BUCKY_CAPS)
	    temp |= 4;
	kbdHwWrite(keyb, keyb->port, temp);
	break;

    default:
        code &= ~0x6000;
	if (code >= RAW_NUM7 && 
	    code <= RAW_NUM0 &&
	    code != 0x4a &&
	    code != 0x4e)
	{
	    if (keyb->keys & KBD_BUCKY_ALT &&
		keypad[code - RAW_NUM7] != -1)
	    {
                if (down)
                {
		    keyb->compose *= 10;
		    keyb->compose += keypad[code - RAW_NUM7];
                }
	    }
	    else if (keyb->keys & KBD_BUCKY_NUM)
		key = '0' + keypad[code - RAW_NUM7];
	    else
		key = keys[code].normal;
	}
        else if ((keyb->keys & (KBD_BUCKY_SHIFT | KBD_BUCKY_ALTGR)) 
            == (KBD_BUCKY_SHIFT | KBD_BUCKY_ALTGR))
	    key = keys[code].altgr_shift;
        else if ((keyb->keys & (KBD_BUCKY_SHIFT | KBD_BUCKY_CTRL)) 
            == (KBD_BUCKY_SHIFT | KBD_BUCKY_CTRL))
	    key = keys[code].control_shift;
        else if (keyb->keys & KBD_BUCKY_ALTGR)
	    key = keys[code].altgr;
        else if (keyb->keys & KBD_BUCKY_CTRL)
	    key = keys[code].control;
        else if (keyb->keys & KBD_BUCKY_SHIFT)
	    key = keys[code].shift;
	else
	    key = keys[code].normal;

	if (keyb->keys & KBD_BUCKY_CAPS)
	{
	    if (iswupper(key))
		key = towlower(key);
	    else if (iswlower(key))
		key = towupper(key);
	}
	
        if (!down)
            key |= KBD_BUCKY_RELEASE;
    }
    
    return key | keyb->keys;
}

uint32_t kbdRead(keyboard_t* keyb)
{
    uint32_t ch;
    
    if (keyb->read == keyb->write)
	return 0;
	
    ch = *keyb->read;
    keyb->read++;
    if (keyb->read >= keyb->buffer + _countof(keyb->buffer))
	keyb->read = keyb->buffer;

    return ch;
}

void KbdStartIo(keyboard_t* keyb)
{
    asyncio_t *io, *ionext;
    request_dev_t *req;
    uint32_t key;
    uint8_t *buf;

    while (keyb->dev.io_first && keyb->read != keyb->write)
    {
	key = kbdRead(keyb);
	for (io = keyb->dev.io_first; io; io = ionext)
	{
	    req = (request_dev_t*) io->req;
	    buf = DevMapBuffer(io);
	    
	    TRACE2("req = %p buffer = %p + %x: ", req, buf);

	    *(uint32_t*) (buf + io->length) = key;
	    DevUnmapBuffer();

	    io->length += sizeof(uint32_t);

	    ionext = io->next;
	    if (io->length >= req->params.dev_read.length)
	    {
		DevFinishIo(&keyb->dev, io, 0);
		TRACE0("finished\n");
	    }
	    else
		TRACE0("more to do\n");
	}
    }
}

bool KbdIsr(device_t *dev, uint8_t irq)
{
    keyboard_t* keyb = (keyboard_t*) dev;
    uint32_t key;
    uint8_t scancode;

    scancode = kbdHwRead(keyb);
    if (keyb->write >= keyb->read)
    {
	key = KbdScancodeToKey(keyb, scancode);

	/*if (key == (KBD_BUCKY_CTRL | KBD_BUCKY_ALT | KEY_DEL))
	    KbdReboot();*/

	//if (key >= KEY_F1 && key <= KEY_F11)
	    //TtySwitchConsoles(key - KEY_F1);
        if (key == KEY_F11)
            __asm__("int3");

        if (key)
	{
	    *keyb->write = key;
	    keyb->write++;
	    TRACE0("[k]");

	    if (keyb->write >= keyb->buffer + _countof(keyb->buffer))
		keyb->write = keyb->buffer;

	    TRACE2("read = %d, write = %d\n", 
		keyb->read - keyb->buffer, keyb->write - keyb->buffer);
	    KbdStartIo(keyb);
	}
    }

    return true;
}

bool kbdRequest(device_t* dev, request_t* req)
{
    keyboard_t* keyb = (keyboard_t*) dev;
    request_dev_t *req_dev = (request_dev_t*) req;
    asyncio_t *io;

    TRACE3("[%c%c/%u]", req->code / 256, req->code % 256, ThrGetCurrent()->id);
    switch (req->code)
    {
    case DEV_READ:
	if (req_dev->params.dev_read.length % sizeof(uint32_t))
	{
	    req_dev->header.code = EINVALID;
	    return false;
	}

	io = DevQueueRequest(dev, 
	    req, 
	    sizeof(request_dev_t),
	    req_dev->params.dev_read.pages,
	    req_dev->params.dev_read.length);
	assert(io != NULL);
	io->length = 0;
	KbdStartIo(keyb);
	return true;

    case DEV_IOCTL:
        if (!MemVerifyBuffer(req_dev->params.dev_ioctl.params, 
            req_dev->params.dev_ioctl.size))
        {
            req_dev->header.result = EBUFFER;
            return false;
        }

        switch (req_dev->params.dev_ioctl.code)
        {
        case IOCTL_BYTES_AVAILABLE:
            TRACE0("keyboard: IOCTL_BYTES_AVAILABLE: ");
            if (req_dev->params.dev_ioctl.size < sizeof(size_t))
            {
                req_dev->header.result = EBUFFER;
                TRACE0("failed\n");
                return false;
            }

            *(size_t*) req_dev->params.dev_ioctl.params = keyb->write - keyb->read;
            TRACE1("%u\n", keyb->write - keyb->read);
            return true;
        }

        req->result = ENOTIMPL;
	return false;
    }

    req->result = ENOTIMPL;
    return false;
}

static const device_vtbl_t keyboard_vtbl =
{
    kbdRequest,
    KbdIsr
};

void KbdAddDevice(driver_t* drv, const wchar_t *name, device_config_t *cfg)
{
    keyboard_t* keyb;
    uint16_t port, ctrl;
    const wchar_t *keymap;
    handle_t file;

    keyb = malloc(sizeof(keyboard_t));

    port = KEYB_PORT;
    ctrl = KEYB_CTRL;

    keyb->dev.driver = drv;
    keyb->dev.vtbl = &keyboard_vtbl;
    keyb->dev.cfg = cfg;
    keyb->dev.io_first = keyb->dev.io_last = NULL;
    keyb->dev.flags = 0;
    keyb->keys = 0;
    keyb->write = keyb->read = keyb->buffer;
    keyb->port = port;
    keyb->ctrl = ctrl;
    keyb->extended = false;

    TRACE2("keyboard: port = %x ctrl = %x\n", port, ctrl);

    /* Reset keyboard and disable scanning until further down */
    TRACE0("Disable...");
    kbdHwWriteRead(keyb, keyb->port, KEYB_RESET_DISABLE, KEYB_ACK);

    /* Set keyboard controller flags: 
       interrupts for keyboard & aux port
       SYS bit (?) */
    TRACE0("done\nFlags...");
    kbdHwWrite(keyb, keyb->ctrl, KCTRL_WRITE_CMD_BYTE);
    kbdHwWrite(keyb, keyb->port, KCTRL_IRQ1 | KCTRL_SYS | KCTRL_IRQ12);

    TRACE0("done\nIdentify...");
    kbdHwWriteRead(keyb, keyb->port, KEYB_IDENTIFY, KEYB_ACK);
    if (kbdHwRead(keyb) == 0xAB &&
	kbdHwRead(keyb) == 0x83)
    {
	TRACE0("[keyboard] PS/2 keyboard detected\n");

	/* Program desired scancode set, expect 0xFA (ACK)... */
	kbdHwWriteRead(keyb, keyb->port, KEYB_SET_SCANCODE_SET, KEYB_ACK);

	/* ...send scancode set value, expect 0xFA (ACK) */
	kbdHwWriteRead(keyb, keyb->port, 1, KEYB_ACK);

	/* make all keys typematic (auto-repeat) and make-break. This may work
	   only with scancode set 3, I'm not sure. Expect 0xFA (ACK) */
	kbdHwWriteRead(keyb, keyb->port, KEYB_ALL_TYPM_MAKE_BRK, KEYB_ACK);

	keyb->isps2 = true;
    }
    else
    {
	/* Argh... bloody bochs */
	TRACE0("[keyboard] AT keyboard detected\n");
	keyb->isps2 = false;
    }

    /* Set typematic delay as short as possible;
       Repeat as fast as possible, expect ACK... */
    TRACE0("Typematic...");
    kbdHwWriteRead(keyb, keyb->port, KEYB_SET_TYPEMATIC, KEYB_ACK);

    /* ...typematic control byte (0 corresponds to MODE CON RATE=30 DELAY=1),
       expect 0xFA (ACK) */
    kbdHwWriteRead(keyb, keyb->port, 0, KEYB_ACK);

    /* Enable keyboard, expect 0xFA (ACK) */
    TRACE0("done\nEnable...");
    kbdHwWriteRead(keyb, keyb->port, KEYB_ENABLE, KEYB_ACK);
    TRACE0("done\n");

    keymap = ProGetString(drv->profile_key, L"DefaultMap", 
        SYS_BOOT L"/british.kbd");
    file = FsOpen(keymap, FILE_READ);
    if (file == NULL)
    {
        wprintf(L"keyboard: unable to open key map file %s\n", keymap);
        __asm__("int3");
    }
    else
    {
        FsReadSync(file, keys, sizeof(keys), NULL);
        HndClose(NULL, file, 'file');
    }

    DevRegisterIrq(1, &keyb->dev);
    DevAddDevice(&keyb->dev, name, cfg);
}

bool DrvInit(driver_t* drv)
{
    drv->add_device = KbdAddDevice;
    return true;
}

/*@} */
