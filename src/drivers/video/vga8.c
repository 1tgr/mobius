/* $Id: vga8.c,v 1.1 2002/03/05 14:23:24 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>

#include <wchar.h>

#include "video.h"
#include "vgamodes.h"

/*! Physical address of the VGA frame buffer */
static uint8_t *video_base = PHYSICAL(0xa0000);
static videomode_t vga8_mode;

uint8_t vga8Dither(int x, int y, colour_t clr)
{
    return (uint8_t) clr;
}

void vga8Close(video_t *vid)
{
}

struct
{
    videomode_t mode;
    const uint8_t *regs;
} vga8_modes[] =
{
    /* width, height, bpp, bpl, regs, cookie */
    { { 320,    200,    8,  0,  0x13, },	mode13h },
};

int vga8EnumModes(video_t *vid, unsigned index, videomode_t *mode)
{
    if (index < _countof(vga8_modes))
    {
	vga8_modes[index].mode.bytesPerLine = 
	    (vga8_modes[index].mode.width * vga8_modes[index].mode.bitsPerPixel) / 8;
	*mode = vga8_modes[index].mode;
	return index == _countof(vga8_modes) - 1 ? VID_ENUM_STOP : VID_ENUM_CONTINUE;
    }
    else
	return VID_ENUM_ERROR;
}

bool vga8SetMode(video_t *vid, videomode_t *mode)
{
    const uint8_t *regs;
    unsigned i;
    
    regs = NULL;
    for (i = 0; i < _countof(vga8_modes); i++)
	if (vga8_modes[i].mode.cookie == mode->cookie)
	{
	    regs = vga8_modes[i].regs;
	    break;
	}
    
    if (regs == NULL)
	return false;

    vga8_mode = vga8_modes[i].mode;
    vgaWriteRegs(regs);
    
    /* Clear the screen when we change modes */
    vid->vidFillRect(vid, 0, 0, vga8_mode.width, vga8_mode.height, 0);
    return true;
}

void vga8PutPixel(video_t *vid, int x, int y, colour_t clr)
{
    video_base[x + y * vga8_mode.bytesPerLine] = vga8Dither(x, y, clr);
}

colour_t vga8GetPixel(video_t *vid, int x, int y)
{
    return video_base[x + y * vga8_mode.bytesPerLine];
}

void vga8HLine(video_t *vid, int x1, int x2, int y, colour_t clr)
{
    memset(video_base + x1 + y * vga8_mode.bytesPerLine, 
	vga8Dither(x1, y, clr),
	x2 - x1);
}

void vga8TextOut(video_t *vid, 
		 int x, int y, vga_font_t *font, const wchar_t *str, 
		 size_t len, colour_t afg, colour_t abg)
{
    int ay, i;
    uint8_t *data, fg, bg, *offset;
    unsigned char ch[2];

    fg = vga8Dither(x, y, afg);
    bg = vga8Dither(x, y, abg);

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
	offset = video_base + x + y * vga8_mode.bytesPerLine;
	
	for (ay = 0; ay < font->Height; ay++)
	{
	    if (afg != (colour_t) -1)
	    	for (i = 0; i < 8; i++)
		    if (data[ay] & (1 << i))
			offset[8 - i] = fg;
	    
	    if (abg != (colour_t) -1)
		for (i = 0; i < 8; i++)
		    if ((data[ay] & (1 << i)) == 0)
			offset[8 - i] = bg;

	    offset += vga8_mode.bytesPerLine;
	}

	x += 8;
    }

    SemRelease(&sem_vga);
}

video_t vga8 =
{
    vga8Close,
    vga8EnumModes,
    vga8SetMode,
    vga8PutPixel,
    vga8GetPixel,
    vga8HLine,
    NULL,	     /* vline */
    NULL,	     /* line */
    NULL,	     /* fillrect */
    vga8TextOut,
    vgaStorePalette
};

video_t *vga8Init(void)
{
    return &vga8;
}
