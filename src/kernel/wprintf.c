#include <stdlib.h>
#include <kernel/kernel.h>
#include <printf.h>
#include <string.h>
#include <kernel/serial.h>
#include <kernel/sys.h>

// I/O addresses for VGA registers
#define VGA_AC_INDEX		0x3C0
#define VGA_AC_WRITE		0x3C0
#define VGA_AC_READ			0x3C1
#define VGA_MISC_WRITE		0x3C2
#define VGA_SEQ_INDEX		0x3C4
#define VGA_SEQ_DATA		0x3C5
#define VGA_DAC_READ_INDEX	0x3C7
#define VGA_DAC_WRITE_INDEX	0x3C8
#define VGA_DAC_DATA		0x3C9
#define VGA_MISC_READ		0x3CC
#define VGA_GC_INDEX		0x3CE
#define VGA_GC_DATA			0x3CF
#define VGA_CRTC_INDEX		0x3D4
#define VGA_CRTC_DATA		0x3D5
#define VGA_INSTAT_READ		0x3DA

// number of registers in each VGA unit
#define NUM_CRTC_REGS	25
#define NUM_AC_REGS		21
#define NUM_GC_REGS		9
#define NUM_SEQ_REGS	5
#define NUM_OTHER_REGS	1
#define NUM_REGS		(NUM_CRTC_REGS + NUM_AC_REGS + NUM_GC_REGS + \
							+ NUM_SEQ_REGS + NUM_OTHER_REGS)

#define _CRTCBaseAdr		0x3c0

static byte _Mode80x25[NUM_REGS]={
/* CRTC */
	0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x4F,
		0x0D, 0x0E, 0x00, 0x00, 0x03, 0x20, 0x9C, 0x8E, 0x8F,
		0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF,
/* attribute controller */
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39,
		0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x0C, 0x00, 0x0F,
		0x08, 0x00,
/* graphics controller */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF,
/* sequencer */
	0x03, 0x00, 0x03, 0x00, 0x02,
/* MISC reg */
	0x67 };

static byte _Mode80x50[NUM_REGS]={
	0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x47,
		0x06, 0x07, 0x00, 0x00, 0x01, 0x40, 0x9C, 0x8E, 0x8F,
		0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39,
		0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x0C, 0x00, 0x0F,
		0x08, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF,
	0x03, 0x00, 0x03, 0x00, 0x02,
	0x67 };

/* ahhh, now this is interesting... */
static byte _Mode90x60[NUM_REGS]={
	0x6B, 0x59, 0x5A, 0x82, 0x60, 0x8D, 0x0B, 0x3E, 0x00, 0x47,
		0x0D, 0x0E, 0x00, 0x00, 0x07, 0x80, 0xEA, 0x0C, 0xDF,
		0x2D, 0x07, 0xE8, 0x05, 0xA3, 0xFF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x38, 0x39,
		0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x08, 0x00, 0x0F,
		0x08, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x0F, 0xFF,
	0x03, 0x01, 0x03, 0x00, 0x02,
	0xE7 };

/* colors 0-7 */
static byte _Pal0[]={
	0, 0, 0,
	0, 0, 168,
	0, 168, 0,
	0, 168, 168,
	168, 0, 0,
	168, 0, 168,
	168, 168, 0,
	168, 168, 168 };

/* colors 56-63 (16-color palette maps these to colors 8-15) */
static byte _Pal1[]={
	84, 84, 84,
	84, 84, 252,
	84, 252, 84,
	84, 252, 252,
	252, 84, 84,
	252, 84, 252,
	252, 252, 84,
	252, 252, 252 };

extern byte _Font8x8Bitmaps[],
	_Font8x16Bitmaps[],
	font1[];

typedef struct
{
	word First, Last;
	byte Height;
	byte *Bitmaps;
} vga_font_t;

static vga_font_t _Font8x8={ 0, 256, 8, _Font8x8Bitmaps };
static vga_font_t _Font8x16={ 0, 256, 16, _Font8x16Bitmaps };

//! Determines the destination of kernel debugging messages, written using 
//!		_cputws() or wprintf()
/*!
 *	Initially this is set to modeBlue, so that the first kernel messages are 
 *		written directly to the screen. After the first kernel driver is 
 *		loaded, this is changed to modeConsole, so that the next _cputws()
 *		will try to open the console driver and write to there.
 */
enum PrintfMode printf_mode = modeBlue;

#ifdef BUFFER
static wchar_t printf_buffer[80], *ch;
#endif

typedef struct textwin_t textwin_t;
struct textwin_t
{
	int x, y;
	int left, top, right, bottom;
	word attrib;
};

int text_width, text_height;
addr_t video_base = 0xb8000;

//!	Pointer to a console driver object, used once the console driver is loaded
//IStream* console;
textwin_t spew, checks;
//! Set to true when wprintf is executing
bool wprintf_lock;
textwin_t *wprintf_window = &spew;

void conUpdateCursor(const textwin_t* win)
{
	unsigned short Off;

	Off = (win->y * text_width + win->x) * 2;
	// extra shift because even/odd text mode uses word clocking
	out(VGA_CRTC_INDEX, 14);
	out(VGA_CRTC_DATA, Off >> 9);
	out(VGA_CRTC_INDEX, 15);
	out(VGA_CRTC_DATA, Off >> 1);
}

void conSetMode(enum PrintfMode mode)
{
	printf_mode = mode;
	//if (mode == modeBlue && console)
		//console->vtbl->Release(console);
}

//! Writes a string directly to the kernel console (in "blue-screen" mode)
/*!
 *	\param	str	The string to be written
 *	\return	Zero
 */
int _cputws_blue(textwin_t* win, const wchar_t* str)
{
	while (*str)
	{
		switch (*str)
		{
		case L'\n':
			win->x = win->left;
			win->y++;
			break;

		case L'\r':
			win->x = win->left;
			break;
		
		case L'\t':
			win->x = (win->x + 4) & ~3;
			break;

		case L'\b':
			if (win->x > 0)
				win->x--;
			break;

		default:
			i386_lpoke16(video_base + (win->y * text_width + win->x) * 2, 
				win->attrib | *str);
			win->x++;
		}

		if (win->x >= win->right)
		{
			win->x = win->left;
			win->y++;
		}

		while (win->y >= win->bottom)
		{
			int x, y;

			for (y = win->top; y < win->bottom; y++)
				for (x = win->left; x < win->right; x++)
					i386_lpoke16(video_base + (y * text_width + x) * 2,
						i386_lpeek16(video_base + ((y + 1) * text_width + x) * 2));

				// xxx - why doesn't this work?
				/*i386_llmemcpy(video_base + (y * text_width + win->left) * 2, 
					video_base + ((y + 1) * text_width + win->left) * 2, 
					(win->right - win->left) * 2);*/
			
			i386_lmemset32(video_base + 2 * (win->left + text_width * (win->bottom - 1)), 
				win->attrib | 0x20 | (win->attrib | 0x20) << 16, 
				(win->right - win->left) * 2);
			win->y--;
		}

		//ioWriteByte(NULL, *str);
		str++;
	}

	conUpdateCursor(win);
	return 0;
}

void _cputws_check(const wchar_t* str)
{
	_cputws_blue(&checks, str);
	conUpdateCursor(&spew);
}

//! Writes a string directly to either the kernel console (in "blue-screen" 
//!		mode) or the console driver
/*!
 *	The value of printf_mode decides where the string will be written.
 *
 *	\param	str	The string to be written
 *	\return	Zero
 */
int _cputws(const wchar_t* str)
{
	/*if (printf_mode == modeConsole &&
		console == NULL)
	{
		IUnknown* unk = sysOpen(L"devices/screen");
		if (unk)
		{
			unk->vtbl->QueryInterface(unk, &IID_IStream, (void**) &console);
			unk->vtbl->Release(unk);
		}

		if (console == NULL)
			printf_mode = modeBlue;
	}

	if (printf_mode == modeBlue)*/
		return _cputws_blue(&spew, str);
	/*else
		return console->vtbl->Write(console, str, sizeof(wchar_t) * wcslen(str));*/
}

wint_t putwchar(wint_t c)
{
	wchar_t str[2] = { c, 0 };
	_cputws(str);
	return c;
}

static bool kprintfhelp(void* pContext, const wchar_t* str, dword len)
{
#ifdef BUFFER
	if (ch + len > printf_buffer + countof(printf_buffer) ||
		*str == L'\r' || *str == L'\n')
	{
		_cputws_blue(wprintf_window, printf_buffer);
		ch = printf_buffer;
	}

	wcscpy(ch, str);
	ch += len;
#else
	_cputws_blue(wprintf_window, str);	
#endif
	return true;
}

int vwprintf(const wchar_t* fmt, va_list ptr)
{
	int ret;

#ifdef BUFFER
	ch = printf_buffer;
#endif

	ret = dowprintf(kprintfhelp, NULL, fmt, ptr);

#ifdef BUFFER
	_cputws_blue(wprintf_window, printf_buffer);
#endif

	return ret;
}

int wprintf(const wchar_t* fmt, ...)
{
	va_list ptr;
	int ret;

	//while (wprintf_lock)
		//;

	wprintf_lock = true;
	va_start(ptr, fmt);
	ret = vwprintf(fmt, ptr);
	va_end(ptr);
	wprintf_lock = false;

	return ret;
}

void nsleep(dword ns)
{
}

static void conSetRegs(byte *Regs)
{
	unsigned Index;

/* enable writes to CRTC regs 0-7 */
	out(VGA_CRTC_INDEX, 17);
	out(VGA_CRTC_DATA,
		in(VGA_CRTC_DATA) & 0x7F);
 /* enable access to vertical retrace regs */
	out(VGA_CRTC_INDEX, 3);
	out(VGA_CRTC_DATA,
		in(VGA_CRTC_DATA) | 0x80);
/* CRTC
make sure we don't disable writes to the CRTC registers: */
	Regs[17] &= 0x7F;   /* undo write-protect on CRTC regs 0-7 */
	Regs[3] |= 0x80;    /* enable access to vertical retrace regs */
	for(Index=0; Index < NUM_CRTC_REGS; Index++)
	{   out(VGA_CRTC_INDEX, Index);
		out(VGA_CRTC_DATA, *Regs++); }
/* attribute controller */
	for(Index=0; Index < NUM_AC_REGS; Index++)
	{   (void)in(VGA_INSTAT_READ);
		nsleep(250);
		out(VGA_AC_INDEX, Index & 0x1F);
		nsleep(250);
		out(VGA_AC_WRITE, *Regs++);
		nsleep(250); }
/* lock palette and unblank display */
	(void)in(VGA_INSTAT_READ);
	nsleep(250);
	out(VGA_AC_INDEX, 0x20);
	nsleep(250);
/* graphics controller */
	for(Index=0; Index < NUM_GC_REGS; Index++)
	{   out(VGA_GC_INDEX, Index);
		out(VGA_GC_DATA, *Regs++); }
/* sequencer */
	for(Index=0; Index < NUM_SEQ_REGS; Index++)
	{   out(VGA_SEQ_INDEX, Index);
		out(VGA_SEQ_DATA, *Regs++); }
/* other regs */
	out(VGA_MISC_WRITE, *Regs);
}

/*****************************************************************************
*****************************************************************************/
void outs16(unsigned short Adr, unsigned short *Data, unsigned Count)
{   for(; Count != 0; Count--)
		out16(Adr, *Data++); }
/*****************************************************************************
*****************************************************************************/
static void conSetFont(vga_font_t *Font)
{/* 0x0100: synchronous reset
0x0402: enable plane 4; disable planes 8, 2, and 1
0x0704: ModeX off, even/odd mode ON (?), >64K on, text mode
0x0300: undo synchronous reset */
	static unsigned short SeqSet[]=
		{ 0x0100, 0x0402, 0x0704, 0x0300 };
/* 0x0204: read mode 0 reads from plane 4
0x0005: 256-color mode off, even/odd mode OFF, read mode 0, write mode 0
0x0C06: screen at B8000, even/odd mode OFF, text mode */
	static unsigned short GCSet[]=
		{ 0x0204, 0x0005, 0x0C06 };
/* 0x0100: synchronous reset
0x0302: enable planes 1 and 2; disable planes 4 and 8
0x0304: Mode X off, even/odd mode OFF, >64K on, text mode
0x0300: undo synchronous reset */
	static unsigned short SeqReset[]=
		{ 0x0100, 0x0302, 0x0304, 0x0300 };
/* 0x0004: read mode 0 reads from none of the four planes
0x1005: 256-color mode off, even/odd mode ON, read mode 0, write mode 0
0x0C06: screen at B8000, even/odd mode ON, text mode */
	static unsigned short GCReset[]=
		{ 0x0004, 0x1005, 0x0E06 };
	byte *Src;
	unsigned Char, Dst;

/* access font memory (plane 4) instead of text memory (planes 2 and 1) */
	outs16(VGA_SEQ_INDEX, SeqSet, sizeof(SeqSet) /
		sizeof(unsigned short));
	outs16(VGA_GC_INDEX, GCSet, sizeof(GCSet) /
		sizeof(unsigned short));
/* program the font */
	Src=Font->Bitmaps + Font->First * Font->Height;
	Dst=Font->First * 32;
	for(Char=Font->First; Char < Font->Last; Char++)
	{
		//movedata(_DS, (unsigned)Src, 0xB800, Dst, Font->Height);
		i386_llmemcpy(video_base + Dst, (addr_t) Src, Font->Height);
		Src += Font->Height;
		Dst += 32;
	}

/* access text memory (planes 2 and 1) instead of font memory (plane 4) */
	outs16(VGA_SEQ_INDEX, SeqReset, sizeof(SeqReset) /
		sizeof(unsigned short));
	outs16(VGA_GC_INDEX, GCReset, sizeof(GCReset) /
		sizeof(unsigned short));
/* set font height in CRTC register 9 */
	out(VGA_CRTC_INDEX, 9);
	out(VGA_CRTC_DATA, (in(VGA_CRTC_DATA) & 0xE0) |
		((Font->Height - 1) & 0x1F));
/* set top and bottom scanlines of cursor */
	if(Font->Height == 16)
	{   out(VGA_CRTC_INDEX, 10);
		out(VGA_CRTC_DATA, 13);
		out(VGA_CRTC_INDEX, 11);
		out(VGA_CRTC_DATA, 14); }
	else if(Font->Height == 8)
	{   out(VGA_CRTC_INDEX, 10);
		out(VGA_CRTC_DATA, 7);
		out(VGA_CRTC_INDEX, 11);
		out(VGA_CRTC_DATA, 7); }}
/*****************************************************************************
*****************************************************************************/
static void conSetPal(unsigned FirstEnt, unsigned Count, byte *Pal)
{   unsigned Index;

/* start with palette entry 0 */
	out(VGA_DAC_WRITE_INDEX, 0);
	for(Index=FirstEnt; Index < FirstEnt + Count; Index++)
	{
		out(VGA_DAC_DATA, *Pal++ >> 2); /* red */
		out(VGA_DAC_DATA, *Pal++ >> 2); /* green */
		out(VGA_DAC_DATA, *Pal++ >> 2); /* blue */
	}
}

static void conDetectSize()
{
	unsigned Temp, VDisp, CharHeight;

	out(VGA_CRTC_INDEX, 0x12);
	VDisp=in(VGA_CRTC_DATA);
	out(VGA_CRTC_INDEX, 0x07);
	Temp=in(VGA_CRTC_DATA);
	if((Temp & 0x02) != 0)
		VDisp |= 0x100;
	if((Temp & 0x40) != 0)
		VDisp |= 0x200;
	VDisp++;

	// scan lines/char
	out(VGA_CRTC_INDEX, 0x09);
	CharHeight=(in(VGA_CRTC_DATA) & 0x1F) + 1;

	// vertical resolution in characters is the quotient
	text_height = VDisp / CharHeight;

	// horizontal resolution in characters
	out(VGA_CRTC_INDEX, 0x01);
	text_width=in(VGA_CRTC_DATA) + 1;
}

void conInit(int nMode)
{
	byte* mode;
	vga_font_t* font;
	int i;
	//int oldWidth = text_width, oldHeight = text_height, i;

	/*switch (nMode)
	{
	case 1:
		mode = _Mode80x25;
		font = &_Font8x16;
		break;
	case 2:
		mode = _Mode80x50;
		font = &_Font8x8;
		break;
	case 3:
		mode = _Mode90x60;
		font = &_Font8x8;
		break;
	default:
		return;
	}

	conSetRegs(mode);
	conSetFont(font);
	conSetPal(0, 8, _Pal0);
	conSetPal(56, 8, _Pal1);
	//conDetectSize();*/
	text_width = 80;
	text_height = 25;

	checks.left = spew.left = checks.x = spew.x = 2;
	checks.right = spew.right = text_width - 2;

	checks.top = checks.y = 1;
	checks.bottom = 10;
	spew.top = spew.y = checks.bottom;
	spew.bottom = text_height - 1;

	checks.attrib = 0x1700;
	spew.attrib = 0x0700;

	/* Clear the screen and test _cputws_blue */
	i386_lmemset32(video_base, 0x17201720UL, text_width * text_height * 2);

	for (i = spew.top; i < spew.bottom; i++)
		i386_lmemset32(video_base + (text_width * i + spew.left) * 2, 
			0x07200720UL, 
			(spew.right - spew.left) * 2);
}