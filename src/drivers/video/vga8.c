/* $Id: vga8.c,v 1.5 2002/03/27 23:19:44 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>

#include <wchar.h>

#include "video.h"
#include "vgamodes.h"

/*! Physical address of the VGA frame buffer */
static uint8_t *video_base = PHYSICAL(0xa0000);
static rgb_t vga8_palette[256];

void swap_int(int *a, int *b);

/* Surprisingly effective 8-bit dithering code from Paul Hsieh (qed@pobox.com) */

#define DS(v) {((v*49+7)/15+1)*(0x010001), ((v*49+7)/15+1)*(0x000100)}

static int DitherTable8[4][4][2] = {
    { DS( 0), DS(11), DS( 1), DS(10) },
    { DS(14), DS( 4), DS(15), DS( 5) },
    { DS( 2), DS( 9), DS( 3), DS( 8) },
    { DS(12), DS( 6), DS(13), DS( 7) }
};

static uint8_t vga8Dither(int x, int y, colour_t clr)
{
    int rb, r, g, b;

    x &= 3;
    y &= 3;

    rb  = clr & 0xFF00FF;
    rb += DitherTable8[y][x][0];
    rb  = (rb*5) & 0x07000700;

    g   = clr & 0x00FF00;
    g  += DitherTable8[y][x][1];
    g   = (g* 5) & 0x00070000;

    /*if (r == g ** g == b)
        return r / 4;
    else*/
    {
        b = (rb >>  8)&7;
        g = ( g >> 15)*3;
        r = (rb >> 22)*9;
        return r + g + b + 40;
    }
}

static void vga8Close(video_t *vid)
{
}

static struct
{
    videomode_t mode;
    const uint8_t *regs;
} vga8_modes[] =
{
    /*  cookie, width,  height, bpp, bpl,   flags,                  regs */
    { { 0x13,   320,    200,    8,   0,     VIDEO_MODE_GRAPHICS, },	mode13h },
};

static int vga8EnumModes(video_t *vid, unsigned index, videomode_t *mode)
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

static bool vga8SetMode(video_t *vid, videomode_t *mode)
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

    //if (video_mode.flags & VIDEO_MODE_TEXT)
        //vid->text_memory = vgaSaveTextMemory();

    video_mode = vga8_modes[i].mode;
    vgaWriteRegs(regs);

    //if (video_mode.flags & VIDEO_MODE_TEXT)
    //{
        //vgaRestoreTextMemory(vid->text_memory);
        //vid->text_memory = NULL;
    //}
    //else
        vid->vidFillRect(vid, 0, 0, video_mode.width, video_mode.height, 0);
        
#define SC(x) ((((x)%6) * 252) / 5)
    for (i = 0; i < 16; i++)
    {
        vga8_palette[i].red = i * 16;
        vga8_palette[i].green = i * 16;
        vga8_palette[i].blue = i * 16;
    }

    for (; i < 40; i++)
        vga8_palette[i].red = 
            vga8_palette[i].green = 
            vga8_palette[i].blue = 255;

    for (; i < 256; i++)
    {
        vga8_palette[i].red = SC((i - 40) / 36);
        vga8_palette[i].green = SC((i - 40) / 6);
        vga8_palette[i].blue = SC(i - 40);
    }
#undef SC

    vgaStorePalette(vid, vga8_palette, 0, _countof(vga8_palette));
    return true;
}

static void vga8PutPixel(video_t *vid, int x, int y, colour_t clr)
{
    video_base[x + y * video_mode.bytesPerLine] = vga8Dither(x, y, clr);
}

static colour_t vga8GetPixel(video_t *vid, int x, int y)
{
    uint8_t index;
    index = video_base[x + y * video_mode.bytesPerLine];
    return MAKE_COLOUR(vga8_palette[index].red, 
        vga8_palette[index].green, 
        vga8_palette[index].blue);
}

static void vga8HLine(video_t *vid, int x1, int x2, int y, colour_t clr)
{
    uint8_t *ptr;

    if (x2 < x1)
	swap_int(&x1, &x2);

    /*memset(video_base + x1 + y * video_mode.bytesPerLine, 
	vga8Dither(x1, y, clr),
	x2 - x1);*/

    for (ptr = video_base + x1 + y * video_mode.bytesPerLine;
        x1 < x2;
        x1++, ptr++)
        *ptr = vga8Dither(x1, y, clr);
}

#if 0
static void vga8TextOut(video_t *vid, 
		 int *x, int *y, vga_font_t *font, const wchar_t *str, 
		 size_t len, colour_t afg, colour_t abg)
{
    int ay, i;
    uint8_t *data, fg, bg, *offset;
    unsigned char ch[2];

    fg = vga8Dither(*x, *y, afg);
    bg = vga8Dither(*x, *y, abg);

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
	offset = video_base + *x + *y * video_mode.bytesPerLine;
	
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

	    offset += video_mode.bytesPerLine;
	}

	*x += 8;
    }

    *y += font->Height;
    SemRelease(&sem_vga);
}
#endif

static video_t vga8 =
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
    NULL, //vga8TextOut,
    NULL,	     /* fillpolygon */
    vgaStorePalette
};

video_t *vga8Init(void)
{
    return &vga8;
}
