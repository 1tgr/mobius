/*
 * $History: rtlsup.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:17
 * Updated in $/coreos/kernel
 * Don't switch to text mode if serial debugger
 * Added history block
 */

#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/sched.h>
#include <kernel/init.h>
#include <kernel/addresses.h>
#include <kernel/debug.h>

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>
#include <malloc.h>

static uint8_t *kernel_sbrk = (uint8_t*) ADDR_HEAP_VIRT;
static unsigned con_x, con_y, con_width = 80, con_height = 25;
static uint16_t con_attribs = 0x0700;
static spinlock_t spin_puts, spin_sbrk;

void vga4TextOut(int *x, int *y, wchar_t max_char, uint8_t *font_bitmaps, 
                 uint8_t font_height, const wchar_t *str, size_t len, 
                 uint32_t afg, uint32_t abg);
void vga4ScrollUp(int pixels, unsigned y, unsigned height);

extern uint8_t font8x8[];

#ifdef MODE_TEXT

/*
 *  Text mode console
 */

#define VGA_AC_INDEX        0x3C0
#define VGA_AC_WRITE        0x3C0
#define VGA_AC_READ         0x3C1
#define VGA_MISC_WRITE      0x3C2
#define VGA_SEQ_INDEX       0x3C4
#define VGA_SEQ_DATA        0x3C5
#define VGA_PALETTE_MASK    0x3C6
#define VGA_DAC_READ_INDEX  0x3C7
#define VGA_DAC_WRITE_INDEX 0x3C8
#define VGA_DAC_DATA        0x3C9
#define VGA_MISC_READ       0x3CC
#define VGA_GC_INDEX        0x3CE
#define VGA_GC_DATA         0x3CF
#define VGA_CRTC_INDEX      0x3D4
#define VGA_CRTC_DATA       0x3D5
#define VGA_INSTAT_READ     0x3DA

static const uint8_t modeK[62] =
{
    /* MISC reg,    STATUS reg,     SEQ regs */
    0x63,           0x00,           0x03, 0x01, 0x03, 0x00, 0x02,
    /* CRTC regs */
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x47, 0x0E, 0x0F, 0x00, 0x00, 0x00, 
    0x00, 0x9C, 0x8E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF, 
    /* GRAPHICS regs */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF, 
    /* ATTRIBUTE CONTROLLER regs */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x10, 0x11, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 
    0x0C, 0x00, 0x0F, 0x00, 0x00
};

static void TextWriteRegs(const uint8_t *regs)
{
    unsigned i;
    volatile uint8_t a;

    /* Send MISC regs */
    out(VGA_MISC_WRITE, *regs++);
    out(VGA_INSTAT_READ, *regs++);
    
    /* Send SEQ regs*/
    for (i = 0; i < 5; i++)
    {
        out(VGA_SEQ_INDEX, i);
        out(VGA_SEQ_DATA, *regs++);
    }
    
    /* Clear Protection bits */
    out16(VGA_CRTC_INDEX, 0xe11);
    
    /* Send CRTC regs */
    for (i = 0; i < 25; i++)
    {
        out(VGA_CRTC_INDEX, i);
        out(VGA_CRTC_DATA, *regs++);
    }

    /* Send GRAPHICS regs */
    for (i = 0; i < 9; i++)
    {
        out(VGA_GC_INDEX, i);
        out(VGA_GC_DATA, *regs++);
    }
    
    a = in(VGA_INSTAT_READ);
    
    /* Send ATTRCON regs */
    for (i = 0; i < 21; i++)
    {
        a = in(VGA_AC_INDEX);
        out(VGA_AC_INDEX, i);
        out(VGA_AC_WRITE, *regs++);
    }
    
    out(VGA_AC_WRITE, 0x20);
}

static void TextSetFont(const uint8_t *bitmaps, unsigned char first, 
                        unsigned char last, unsigned height)
{
/* 0x0100: synchronous reset
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
	const unsigned char *Src;
	unsigned Char, Dst;
    unsigned i;

/* access font memory (plane 4) instead of text memory (planes 2 and 1) */
    for (i = 0; i < _countof(SeqSet); i++)
	    out16(VGA_SEQ_INDEX, SeqSet[i]);
    for (i = 0; i < _countof(GCSet); i++)
	    out16(VGA_GC_INDEX, GCSet[i]);

/* program the font */
	Src=bitmaps + first * height;
	Dst=first * 32;
	for(Char=first; Char < last; Char++)
    {
        /*movedata(_DS, (unsigned)Src, 0xB800, Dst, Font->Height);*/
        memcpy((char*) PHYSICAL(0xb8000) + Dst, Src, height);
		Src += height;
		Dst += 32;
    }
/* access text memory (planes 2 and 1) instead of font memory (plane 4) */
    for (i = 0; i < _countof(SeqReset); i++)
	    out16(VGA_SEQ_INDEX, SeqReset[i]);
    for (i = 0; i < _countof(GCReset); i++)
	    out16(VGA_GC_INDEX, GCReset[i]);
/* set font height inportb CRTC register 9 */
	out(VGA_CRTC_INDEX, 9);
	out(VGA_CRTC_DATA, (in(VGA_CRTC_DATA) & 0xE0) |
		((height - 1) & 0x1F));
/* set top and bottom scanlines of cursor */
	if(height == 16)
	{	out(VGA_CRTC_INDEX, 10);
		out(VGA_CRTC_DATA, 13);
		out(VGA_CRTC_INDEX, 11);
		out(VGA_CRTC_DATA, 14);
	}
	else if(height == 8)
	{	out(VGA_CRTC_INDEX, 10);
		out(VGA_CRTC_DATA, 7);
		out(VGA_CRTC_INDEX, 11);
		out(VGA_CRTC_DATA, 7);
	}
}

static void TextUpdateCursor(void)
{
    unsigned short Off;

    Off = (con_y * 80 + con_x) * 2;
    /* extra shift because even/odd text mode uses word clocking */
    out(VGA_CRTC_INDEX, 14);
    out(VGA_CRTC_DATA, Off >> 9);
    out(VGA_CRTC_INDEX, 15);
    out(VGA_CRTC_DATA, Off >> 1);
}

void TextSwitchToKernel(void)
{
    TextWriteRegs(modeK);
    TextSetFont(font8x8, 0, 255, 8);
    out(VGA_CRTC_INDEX, 12);
    out(VGA_CRTC_DATA, 0);
    out(VGA_CRTC_INDEX, 13);
    out(VGA_CRTC_DATA, 0);
    TextUpdateCursor();
}

void TextUpdateProgress(int min, int progress, int max)
{
    uint16_t *mem;
    unsigned chars, i;

    if (min == max)
        chars = 0;
    else
        chars = ((progress - min) * con_width) / (max - min);

    mem = (uint16_t*) PHYSICAL(0xb8000) + con_width * (con_height - 1);
    for (i = 0; i < chars; i++)
        mem[i] = 0x71fe;
    for (; i < con_width; i++)
        mem[i] = 0x7120;
}

#define DEFINE_PUTS(name, ct) \
int name(const ct *str, size_t count) \
{ \
    uint16_t *mem = (uint16_t*) PHYSICAL(0xb8000); \
\
    SpinAcquire(&spin_puts); \
    for (; *str && count > 0; count--, str++) \
    { \
        switch (*str) \
        { \
        case '\n': \
            KeAtomicInc(&con_y); \
        case '\r': \
            con_x = 0; \
            break; \
 \
        case '\b': \
            if (con_x > 0) \
                KeAtomicDec(&con_x); \
            break; \
 \
        case '\t': \
            con_x = (con_x + 4) & ~3; \
            break; \
 \
        default: \
            mem[con_x + con_y * con_width] =  \
                (uint16_t) (uint8_t) *str | con_attribs; \
            KeAtomicInc(&con_x); \
        } \
 \
        if (con_x >= con_width) \
        { \
            con_x = 0; \
            KeAtomicInc(&con_y); \
        } \
 \
        while (con_y >= con_height - 1) \
        { \
            unsigned i; \
 \
            memmove(mem, mem + con_width, con_width * (con_height - 2) * 2); \
            for (i = 0; i < con_width; i++) \
                mem[con_width * (con_height - 2) + i] = ' ' | con_attribs; \
 \
            KeAtomicDec(&con_y); \
        } \
    } \
 \
    TextUpdateCursor(); \
    SpinRelease(&spin_puts); \
    return 0; \
}

#else

/*
 *  Graphical console
 */

#include "../drivers/video/include/video.h"

/* Hard-code BIOS mode 12h */
videomode_t video_mode = { 0x12 };
video_t *vga4Init(device_config_t *cfg);

video_t *con_vga4;
void vga4DisplayBitmap(unsigned x, unsigned y, unsigned width, 
                       unsigned height, void *data);

static const rgb_t vga_palette[3 * 16] =
{
    { 0,    0,    0,    },
    { 128,  0,    0,    },
    { 0,    128,  0,    },
    { 128,  128,  0,    },
    { 0,    0,    128,  },
    { 128,  0,    128,  },
    { 0,    128,  128,  },
    { 192,  192,  192,  },
    { 128,  128,  128,  },
    { 255,  0,    0,    },
    { 0,    255,  0,    },
    { 255,  255,  0,    },
    { 0,    0,    255,  },
    { 255,  0,    255,  },
    { 0,    255,  255,  },
    { 255,  255,  255,  },
};

void TextUpdateProgress(int min, int progress, int max)
{
    unsigned x, y, top, bottom;

    if (max == min)
        x = 0;
    else
        x = (progress * 640) / (max - min);

    top = (con_top + con_height - 1) * 8 + 2;
    bottom = (con_top + con_height) * 8 - 2;
    for (y = top; y < bottom; y++)
    {
        con_vga4->vidHLine(con_vga4, 2, x, y, 4);
        con_vga4->vidHLine(con_vga4, x, 638, y, 7);
    }
}

static wchar_t *con_buffer[4];
static unsigned con_page;

void TextWrite(unsigned page, unsigned row, unsigned col, const wchar_t *str, 
               size_t length, uint16_t attribs)
{
    int x, y;

    //assert(page < _countof(con_buffer));
    if (con_buffer[page] == NULL)
        con_buffer[page] = malloc(sizeof(wchar_t) * con_width * con_height);
    //assert(con_buffer[page] != NULL);

    memcpy(con_buffer[page] + row * con_width + col, str, 
        length * sizeof(wchar_t));
    if (page == con_page)
    {
        x = col * 8;
        y = row * 8;
        vga4TextOut(&x, &y, 256U, font8x8, 8, str, length, 
            (attribs >> 0) & 0xf, (attribs >> 4) & 0xf);
    }
}

void TextSetVisiblePage(unsigned page)
{
    unsigned row;
    int x, y;

    if (page != con_page)
        con_page = page;

    if (con_buffer[page] != NULL)
    {
        x = 0;
        for (row = 0; row < con_height; row++)
        {
            y = (row + con_top) * 8;
            vga4TextOut(&x, &y, 256U, font8x8, 8, 
                con_buffer[page] + row * con_width,
                con_width,
                7, 0);
        }
    }
}

void TextSwitchToKernel(void)
{
    TextSetVisiblePage(0);
}

#define DEFINE_PUTS(name, ct) \
int name(const ct *str, size_t count) \
{ \
    int y; \
    wchar_t temp[2] = { 0 }; \
\
    SpinAcquire(&spin_puts); \
    for (; *str && count > 0; count--, str++) \
    { \
        switch (*str) \
        { \
        case '\n': \
            KeAtomicInc(&con_y); \
        case '\r': \
            con_x = 0; \
            break; \
\
        case '\b': \
            if (con_x > 0) \
                KeAtomicDec(&con_x); \
            break; \
\
        case '\t': \
            con_x = (con_x + 4) & ~3; \
            break; \
\
        default: \
            temp[0] = (wchar_t) *str; \
            TextWrite(0, con_y + con_top, con_x, temp, 1, con_attribs >> 8); \
            KeAtomicInc(&con_x); \
        } \
\
        if (con_x >= con_width) \
        { \
            con_x = 0; \
            KeAtomicInc(&con_y); \
        } \
\
        if (con_y >= con_height - 1) \
        { \
            vga4ScrollUp((con_y - con_height + 2) * 8, con_top * 8, (con_height - 1) * 8); \
            for (y = 0; y < 8; y++) \
                con_vga4->vidHLine(con_vga4, 0, 640, \
                    (con_top + con_height - 2) * 8 + y, (con_attribs >> 12) & 0xf); \
            con_y = con_height - 2; \
        } \
    } \
\
    SpinRelease(&spin_puts); \
    return 0; \
}

#endif


void assert_failed(const char *test, const char *file, int line)
{
    if (kernel_startup.debug_port <= 1)
        DbgSetPort(1, 0);
    wprintf(L"Assertion failed: %S, line %d\n"
        L"%S\n",
        file, line, test);
}


DEFINE_PUTS(DbgWriteStringConsole, wchar_t);

wchar_t *ProcGetCwd()
{
    return current()->process->info->cwd;
}


void *sbrk_virtual(size_t diff)
{
    uint8_t *new_sbrk, *start;

    diff = PAGE_ALIGN_UP(diff);
    assert(diff < 0x01000000);

	SpinAcquire(&spin_sbrk);
	start = kernel_sbrk;
	new_sbrk = kernel_sbrk + diff;

    if (new_sbrk >= (uint8_t*) ADDR_TEMP_VIRT)
	{
		SpinRelease(&spin_sbrk);
        return NULL;
	}

	kernel_sbrk = new_sbrk;
	SpinRelease(&spin_sbrk);
    return start;
}


void *__morecore(size_t diff)
{
    uint8_t *start, *virt, *end;
	addr_t phys;

	diff = PAGE_ALIGN_UP(diff);
	assert(diff < 0x00100000);
	start = sbrk_virtual(diff);
	if (start == NULL)
		return NULL;

	virt = start;
	end = start + diff;
    while (virt < end)
    {
        phys = MemAlloc();
        if (phys == NULL)
            return NULL;

        if (!MemMapRange(virt, 
			phys, 
			virt + PAGE_SIZE, 
            PRIV_WR | PRIV_KERN | PRIV_PRES))
            return NULL;

		virt += PAGE_SIZE;
    }

    return start;
}


int *_geterrno()
{
    assert(current()->info != NULL);
    return &current()->info->status;
}


const __wchar_info_t *__lookup_unicode(wchar_t cp)
{
    return NULL;
}


int __malloc_lock(int lock_unlock, unsigned long *lock)
{
	if (lock_unlock)
		SpinAcquire((spinlock_t*) lock);
	else
		SpinRelease((spinlock_t*) lock);

	return 0;
}

#ifndef MODE_TEXT
static wchar_t con_buffer0[80 * 25];
#endif

bool DbgInitConsole(void)
{
    unsigned i;

#ifdef MODE_TEXT
    uint16_t *ptr;

	con_x = 0;
	con_y = 0;
    con_width = 80;
    con_height = 50;

    TextWriteRegs(modeK);
    TextSetFont(font8x8, 0, 255, 8);

    ptr = PHYSICAL(0xb8000);
    for (i = 0; i < con_width * con_height; i++)
        ptr[i] = con_attribs | ' ';
#else
    addr_t bmp;

    con_width = 80;
    con_height = 25;
    con_top = 35;
    con_buffer[0] = con_buffer0;

    con_vga4 = vga4Init(NULL);
    con_vga4->vidSetMode(con_vga4, &video_mode);

    vgaStorePalette(NULL, vga_palette, 0, _countof(vga_palette));
    bmp = RdGetFilePhysicalAddress(L"kernel.raw", NULL);
    if (bmp == NULL)
        for (i = 0; i < 280; i++)
            con_vga4->vidHLine(con_vga4, 0, 640, i, 15);
    else
        vga4DisplayBitmap(0, 0, 640, 280, PHYSICAL(bmp));

    for (i = 280; i < 472; i++)
        con_vga4->vidHLine(con_vga4, 0, 640, i, 0);
    for (;        i < 480; i++)
        con_vga4->vidHLine(con_vga4, 0, 640, i, 7);
#endif

    return true;
}
