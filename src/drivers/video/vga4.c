/* $Id: vga4.c,v 1.5 2002/03/05 14:23:24 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>

#include <wchar.h>

#include "video.h"
#include "vgamodes.h"

/*! Physical address of the VGA frame buffer */
static uint8_t *video_base = PHYSICAL(0xa0000);
static int maskbit[640], y80[480], xconv[640], startmasks[8], endmasks[8];

uint8_t vga4Dither(int x, int y, colour_t clr)
{
    return (uint8_t) clr;
}

void vga4Close(video_t *vid)
{
}

struct
{
    videomode_t mode;
    const uint8_t *regs;
} vga4_modes[] =
{
    /* width, height, bpp, bpl, regs, cookie */
    { { 80,	25,	4,  0,	    0x03, },  mode03h },
  /*{ { 320,    200,    2,  8192,   0x04, },	mode04h },
    { { 640,    200,    1,  8192,   0x06, },	mode06h },*/
  /*{ { 320,    200,    4,  8000,   0x0d, },	mode0Dh },*/ /* xxx - doesn't work */
    { { 640,    200,    4,  16000,  0x0e, },	mode0Eh },
  /*{ { 640,    350,    1,  28000,  0x0f, },	mode0Fh },*/
    { { 640,    350,    4,  28000,  0x10, },	mode10h },
  /*{ { 640,    480,    1,  38400,  0x11, },	mode11h },*/
    { { 640,    480,    4,  38400,  0x12, },	mode12h },
};

int vga4EnumModes(video_t *vid, unsigned index, videomode_t *mode)
{
    if (index < _countof(vga4_modes))
    {
	vga4_modes[index].mode.bytesPerLine = 
	    (vga4_modes[index].mode.width * vga4_modes[index].mode.bitsPerPixel) / 8;
	*mode = vga4_modes[index].mode;
	return index == _countof(vga4_modes) - 1 ? VID_ENUM_STOP : VID_ENUM_CONTINUE;
    }
    else
	return VID_ENUM_ERROR;
}

void vga4PreCalc(void)
{
    unsigned long j;

    startmasks[7] = 255;
    startmasks[6] = 127;
    startmasks[5] = 63;
    startmasks[4] = 31;
    startmasks[3] = 15;
    startmasks[2] = 7;
    startmasks[1] = 3;
    startmasks[0] = 1;

    endmasks[0] = 255;
    endmasks[1] = 128;
    endmasks[2] = 192;
    endmasks[3] = 224;
    endmasks[4] = 240;
    endmasks[5] = 248;
    endmasks[6] = 252;
    endmasks[7] = 254;
    
    for (j = 0; j < 80; j++)
    {
	maskbit[j * 8] = 128;
	maskbit[j * 8 + 1] = 64;
	maskbit[j * 8 + 2] = 32;
	maskbit[j * 8 + 3] = 16;
	maskbit[j * 8 + 4] = 8;
	maskbit[j * 8 + 5] = 4;
	maskbit[j * 8 + 6] = 2;
	maskbit[j * 8 + 7] = 1;
    }
    
    for (j = 0; j < 480; j++)
	y80[j] = j * 80;
    for (j = 0; j < 640; j++)
	xconv[j] = j >> 3;
}

bool vga4SetMode(video_t *vid, videomode_t *mode)
{
    const uint8_t *regs;
    unsigned i;
    
    regs = NULL;
    for (i = 0; i < _countof(vga4_modes); i++)
	if (vga4_modes[i].mode.cookie == mode->cookie)
	{
	    regs = vga4_modes[i].regs;
	    break;
	}
    
    if (regs == NULL)
	return false;

    /*vgaWriteRegs(regs);*/
    
    /* Clear the screen when we change modes */
    vga4PreCalc();
    vid->vidFillRect(vid, 0, 0, mode->width, mode->height, 0);
    return true;
}

void vga4PutPixel(video_t *vid, int x, int y, colour_t clr)
{
    uint8_t *offset;
    volatile uint8_t a;
    uint8_t pix;

    pix = vga4Dither(x, y, clr);
    offset = video_base + xconv[x] + y80[y];

    SemAcquire(&sem_vga);
    out16(VGA_GC_INDEX, 0x08 | (maskbit[x] << 8));
    if (pix)
    {
	out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
	a = *offset;
	*offset = 0xff;
    }

    if (~pix)
    {
	out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
	a = *offset;
	*offset = 0;
    }

    SemRelease(&sem_vga);
}

void vga4GetByte(addr_t offset,
		 uint8_t *b, uint8_t *g,
		 uint8_t *r, uint8_t *i)
{
    SemAcquire(&sem_vga);
    out16(VGA_GC_INDEX, 0x0304);
    *i = video_base[offset];
    out16(VGA_GC_INDEX, 0x0204);
    *r = video_base[offset];
    out16(VGA_GC_INDEX, 0x0104);
    *g = video_base[offset];
    out16(VGA_GC_INDEX, 0x0004);
    *b = video_base[offset];
    SemRelease(&sem_vga);
}

colour_t vga4GetPixel(video_t *vid, int x, int y)
{
    uint8_t mask, b, g, r, i;

    vga4GetByte(xconv[x] + y80[y], &b, &g, &r, &i);

    mask = maskbit[x];
    b &= mask;
    g &= mask;
    r &= mask;
    i &= mask;

    mask = 7 - (x % 8);
    g >>= mask;
    b >>= mask;
    r >>= mask;
    i >>= mask;

    return b + 2 * g + 4 * r + 8 * i;
}

void vga4HLine(video_t *vid, int x1, int x2, int y, colour_t clr)
{
    int midx, leftpix, rightx, midpix, rightpix;
    uint8_t leftmask, rightmask, pix, *offset;
    volatile uint8_t a;

    pix = vga4Dither(x1, y, clr);
    offset = video_base;
    offset += xconv[x1] + y80[y];
    
    /* midx = start of middle region */
    midx = (x1 + 7) & -8;
    /* leftpix = number of pixels to left of middle */
    leftpix = midx - x1;

    SemAcquire(&sem_vga);
    if (leftpix > 0)
    {
	/* leftmask = pixels set to left of middle */
	leftmask = 0xff >> (8 - leftpix);
	/*leftmask = startmasks[leftpix];*/

	out16(VGA_GC_INDEX, 0x08 | (leftmask << 8));
	out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
	a = *offset;
	*offset = 0xff;
	out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
	a = *offset;
	*offset = 0;

	offset++;
    }

    /* rightx = end of middle region */
    rightx = x2 & -8;
    /* midpix = number of pixels in middle */
    midpix = rightx - midx;

    out16(VGA_GC_INDEX, 0xff08);
    if (pix)
    {
	out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
	memset(offset, 0xff, midpix / 8);
    }

    if (~pix)
    {
	out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
	memset(offset, 0, midpix / 8);
    }

    offset += midpix / 8;

    /* rightpix = number of pixels to right of middle */
    rightpix = x2 - rightx;
    if (rightpix > 0)
    {
	/* rightmask = pixels set to right of middle */
	rightmask = 0xff << (8 - rightpix);
	/*rightmask = endmasks[rightpix];*/

	out16(VGA_GC_INDEX, 0x08 | (rightmask << 8));
	out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
	a = *offset;
	*offset = 0xff;
	out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
	a = *offset;
	*offset = 0;
    }

    SemRelease(&sem_vga);
}

void vga4TextOut(video_t *vid, 
		 int x, int y, vga_font_t *font, const wchar_t *str, 
		 size_t len, colour_t afg, colour_t abg)
{
    int ay;
    uint8_t *data, fg, bg, *offset;
    volatile int a;
    unsigned char ch[2];

    fg = vga4Dither(x, y, afg);
    bg = vga4Dither(x, y, abg);

    if (len == -1)
	len = wcslen(str);

    SemAcquire(&sem_vga);
    for (; len > 0; str++, len--)
    {
	ch[0] = 0;
	wcsto437(ch, str, 1);
	if (ch[0] < font->First || ch[0] > font->Last)
	    ch[0] = '?';

	data = font->Bitmaps + font->Height * (ch[0] - font->First);
	offset = video_base + xconv[x] + y80[y];
	
	for (ay = 0; ay < font->Height; ay++)
	{
	    if (afg != (colour_t) -1)
	    {
		/* draw letter */
		out16(VGA_GC_INDEX, 0x08 | (data[ay] << 8));
		if (fg)
		{
		    out16(VGA_SEQ_INDEX, 0x02 | (fg << 8));
		    a = *offset;
		    *offset = 0xff;
		}

		if (~fg)
		{
		    out16(VGA_SEQ_INDEX, 0x02 | (~fg << 8));
		    a = *offset;
		    *offset = 0;
		}
	    }

	    if (abg != (colour_t) -1)
	    {
		/* draw background */
		out16(VGA_GC_INDEX, 0x08 | (~data[ay] << 8));
		if (bg)
		{
		    out16(VGA_SEQ_INDEX, 0x02 | (bg << 8));
		    a = *offset;
		    *offset = 0xff;
		}

		if (~bg)
		{
		    out16(VGA_SEQ_INDEX, 0x02 | (~bg << 8));
		    a = *offset;
		    *offset = 0;
		}
	    }

	    offset += 80;
	}

	x += 8;
    }

    SemRelease(&sem_vga);
}

video_t vga4 =
{
    vga4Close,
    vga4EnumModes,
    vga4SetMode,
    vga4PutPixel,
    vga4GetPixel,
    vga4HLine,
    NULL,	     /* vline */
    NULL,	     /* line */
    NULL,	     /* fillrect */
    vga4TextOut,
    vgaStorePalette
};

video_t *vga4Init(void)
{
    return &vga4;
}
