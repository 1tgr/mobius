#include <stdlib.h>

#ifdef _DEUB
#define TRACE0(f)		printf(f)
#define TRACE1(f, a)	printf(f, a)
#define TRACE2(f, a, b)	printf(f, a, b)
#else
#define TRACE0(f)
#define TRACE1(f, a)
#define TRACE2(f, a, b)
#endif

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

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef enum { false, true } bool;

#define msleep thrSleep

/*
 * Ask the OS to call func() when the specified IRQ occurs.
 * The parameter provided (context) will be passed to the function.
 */
void sysRegisterIrq(int irq, void (__cdecl *func)(void*, int), void* context);

/* Pauses execution for ms milliseconds */
void msleep(unsigned ms);

#ifndef min
#define min(a, b)	((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b)	((a) > (b) ? (a) : (b))
#endif

#ifdef _MSC_VER

#pragma warning(disable:4035)

#define enable()	__asm sti

byte in(word port)
{
	__asm
	{
		xor	eax, eax
		mov	dx, port
		in	al, dx
	}
}

void out(word port, byte data)
{
	__asm
	{
		movzx	eax, data
		mov	dx, port
		out	dx, al
	}
}

#else

static inline void enable() { asm("sti"); }

static inline byte in(word port)
{
	byte ret;
	asm("movw	%1,%%dx ;"
		"inb	%%dx,%%al ;"
		"movb	%%al,%0" : "=r" (ret) : "r" (port) : "eax");
	return ret;
}

#define out(port, value) \
	asm("outb	%%al, %%dx" : : "d" (port), "a" (value) : "eax", "edx")

#endif

static word interrupts;

typedef struct Ps2Mouse Ps2Mouse;
struct Ps2Mouse
{
	bool has_wheel;
	int x, y, xmax, xmin, ymax, ymin, wheel;
	dword buttons;
};

void kbdWrite(word port, byte data)
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

byte kbdWriteRead(word port, byte data, const char* expect)
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

void ps2Isr(void* ctx, int irq);

Ps2Mouse* ps2Init()
{
	/* These strings nicked from gpm (I_imps2): I don't know how they work... */
	static byte s1[] = { 0xF3, 0xC8, 0xF3, 0x64, 0xF3, 0x50, 0 };
	static byte s2[] = { 0xF6, 0xE6, 0xF4, 0xF3, 0x64, 0xE8, 0x03, 0 };
	Ps2Mouse* ctx;
	const byte* ch;
	byte id;

	ctx = (Ps2Mouse*) malloc(sizeof(Ps2Mouse));

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
	TRACE1(L"[mouse] status = %d\n", kbdRead());
	TRACE1(L"[mouse] resolution = %d\n", kbdRead());
	TRACE1(L"[mouse] sample rate = %d\n", kbdRead());

	/* enable aux device (mouse) */
	kbdWrite(KEYB_CTRL, KCTRL_WRITE_AUX);
	kbdWriteRead(KEYB_PORT, AUX_ENABLE, KEYB_ACK);

	sysRegisterIrq(12, ps2Isr, ctx);
	
	ctx->xmin = ctx->ymin = 0;
	ctx->xmax = 320;
	ctx->ymax = 200;
	ctx->x = (ctx->xmin + ctx->xmax) / 2;
	ctx->y = (ctx->ymin + ctx->ymax) / 2;

	return ctx;
}

void ps2Delete(Ps2Mouse* ctx)
{
	/* may need to shut down hardware here */

	sysRegisterIrq(12, NULL, NULL);
	free(ctx);
}

byte ps2AuxRead()
{
	byte Stat, Data;

	enable();

	while (true)
	{
		while ((interrupts & 0x1000) == 0)
			;

		interrupts &= ~0x1000;
		
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

	/*putwchar(buf[0]);
	putwchar(buf[1]);
	putwchar(buf[2]);
	putwchar(buf[3]);*/

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

	ctx->x += dx;
	ctx->y += dy;

	ctx->x = min(ctx->x, ctx->xmax);
	ctx->x = max(ctx->x, ctx->xmin);
	ctx->y = min(ctx->y, ctx->ymax);
	ctx->y = max(ctx->y, ctx->ymin);
	ctx->buttons = but;
	ctx->wheel += dw;
}

void ps2Isr(void* ctx, int irq)
{
	static int in_isr = 0;
	byte stat;
	
	stat = in(KEYB_CTRL);
	if ((stat & 0x01) != 0)
	{
		interrupts |= 1 << irq;
		if (!in_isr)
		{
			in_isr++;
			ps2Packet((Ps2Mouse*) ctx);
			in_isr--;
		}
	}
}
