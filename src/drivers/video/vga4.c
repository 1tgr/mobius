#include <kernel/kernel.h>
#include <kernel/arch.h>

#include <wchar.h>

#include "video.h"
#include "vgamodes.h"

size_t wcsto437(char *mbstr, const wchar_t *wcstr, size_t count);

/*! Physical address of the VGA frame buffer */
uint8_t *video_base = PHYSICAL(0xa0000);
int maskbit[640], y80[480], xconv[640], startmasks[8], endmasks[8];

uint8_t vga4Dither(int x, int y, colour_t clr)
{
    return (uint8_t) clr;
}

void vga4Close(video_t *vid)
{
}

const struct
{
    videomode_t mode;
    const uint8_t *regs;
} vga4_modes[] =
{
    /* cookie, width, height, bpp, bpl, regs */
    /*{ { 0x04,    320,    200,    2, 8192    },	mode04h },
    { { 0x06,    640,    200,    1, 8192    },	mode06h },*/
    /*{ { 0x0d,    320,    200,    4, 8000    },	mode0Dh },*/ /* xxx - doesn't work */
    { { 0x0e,    640,    200,    4, 16000   },	mode0Eh },
    /*{ { 0x0f,    640,    350,    1, 28000   },	mode0Fh },*/
    { { 0x10,    640,    350,    4, 28000   },	mode10h },
    /*{ { 0x11,    640,    480,    1, 38400   },	mode11h },*/
    { { 0x12,    640,    480,    4, 38400   },	mode12h },
    /*{ { 0x13,    320,    200,    8, 64000   },	mode13h },*/
};

int vga4EnumModes(video_t *vid, unsigned index, videomode_t *mode)
{
    if (index < _countof(vga4_modes))
    {
	*mode = vga4_modes[index].mode;
	mode->bytesPerLine = (mode->width * mode->bitsPerPixel) / 8;
	return index == _countof(vga4_modes) - 1 ? VID_ENUM_STOP : VID_ENUM_CONTINUE;
    }
    else
	return VID_ENUM_ERROR;
}

bool vga4SetMode(video_t *vid, videomode_t *mode)
{
    const uint8_t *regs;
    unsigned i;
    volatile uint8_t a;
    extern vga_font_t _Cupertino;

    regs = NULL;
    for (i = 0; i < _countof(vga4_modes); i++)
	if (vga4_modes[i].mode.cookie == mode->cookie)
	{
	    regs = vga4_modes[i].regs;
	    break;
	}
    
    if (regs == NULL)
	return false;

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
    
    /* Clear the screen when we "change" modes */
    vid->vidFillRect(vid, 0, 0, mode->width, mode->height, 1);
    vid->vidLine(vid, 0, 0, mode->width, mode->height, 2);
    vid->vidTextOut(vid, 100, 100, &_Cupertino, L"Hello, world!", 13, 0, 1);
    return true;
}

void vga4PreCalc()
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

void vga4PutPixel(video_t *vid, int x, int y, colour_t clr)
{
    uint8_t *offset;
    volatile uint8_t a;
    uint8_t pix;

    pix = vga4Dither(x, y, clr);
    offset = video_base + xconv[x] + y80[y];

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
}

void vga4GetByte(addr_t offset,
		 uint8_t *b, uint8_t *g,
		 uint8_t *r, uint8_t *i)
{
    out16(VGA_GC_INDEX, 0x0304);
    *i = video_base[offset];
    out16(VGA_GC_INDEX, 0x0204);
    *r = video_base[offset];
    out16(VGA_GC_INDEX, 0x0104);
    *g = video_base[offset];
    out16(VGA_GC_INDEX, 0x0004);
    *b = video_base[offset];
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
}

void vga4StorePalette(video_t *vid, const rgb_t *entries, unsigned first,
		      unsigned count)
{
    unsigned Index;

    /* start with palette entry 0 */
    out(VGA_DAC_WRITE_INDEX, first);
    for (Index = 0; Index < count; Index++)
    {
	out(VGA_DAC_DATA, entries[Index].red >> 2); /* red */
	out(VGA_DAC_DATA, entries[Index].green >> 2); /* green */
	out(VGA_DAC_DATA, entries[Index].blue >> 2); /* blue */
    }
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
    vga4StorePalette
};

video_t *vga4Init(videomode_t *mode)
{
    vga4PreCalc();
    vga4SetMode(&vga4, mode);
    return &vga4;
}