/* $Id: bpp8.c,v 1.2 2002/12/18 23:06:10 pavlovskii Exp $ */

#include <os/video.h>
#include "internal.h"
#include "bpp8.h"

extern uint8_t *video_base;
extern videomode_t gmgr_mode;

/* Surprisingly effective 8-bit dithering code from Paul Hsieh (qed@pobox.com) */

#define DS(v) {((v*49+7)/15+1)*(0x010001), ((v*49+7)/15+1)*(0x000100)}

static rgb_t bpp8_palette[256];
static int DitherTable8[4][4][2] = {
    { DS( 0), DS(11), DS( 1), DS(10) },
    { DS(14), DS( 4), DS(15), DS( 5) },
    { DS( 2), DS( 9), DS( 3), DS( 8) },
    { DS(12), DS( 6), DS(13), DS( 7) }
};

static uint8_t bpp8_dtbl[8 * 8];
static MGLcolour bpp8_last_colour;

static void Bpp8GeneratePalette(rgb_t *palette)
{
    int i;

#define SC(x) ((((x)%6) * 252) / 5)
    for (i = 0; i < 16; i++)
    {
        palette[i].red = i * 16;
        palette[i].green = i * 16;
        palette[i].blue = i * 16;
    }

    for (; i < 40; i++)
        palette[i].red = 
            palette[i].green = 
            palette[i].blue = 255;

    for (; i < 256; i++)
    {
        palette[i].red = SC((i - 40) / 36);
        palette[i].green = SC((i - 40) / 6);
        palette[i].blue = SC(i - 40);
    }
#undef SC
}

static uint8_t Bpp8Dither(int x, int y, MGLcolour clr)
{
    int rb, r, g, b;

    if (MGL_RED(clr) / 16 == MGL_GREEN(clr) / 16 &&
        MGL_GREEN(clr) / 16 == MGL_BLUE(clr) / 16)
        return MGL_RED(clr) / 16;

    x &= 3;
    y &= 3;

    rb  = clr & 0xFF00FF;
    rb += DitherTable8[y][x][0];
    rb  = (rb*5) & 0x07000700;

    g   = clr & 0x00FF00;
    g  += DitherTable8[y][x][1];
    g   = (g* 5) & 0x00070000;

    b = (rb >>  8)&7;
    g = ( g >> 15)*3;
    r = (rb >> 22)*9;
    return r + g + b + 40;
}

static uint8_t *Bpp8PrepareDitherTable(MGLcolour clr)
{
    unsigned x, y;

    if (bpp8_last_colour != clr)
    {
        for (x = 0; x < 8; x++)
            for (y = 0; y < 8; y++)
                bpp8_dtbl[y * 8 + x] = Bpp8Dither(x, y, clr);

        bpp8_last_colour = clr;
    }

    return bpp8_dtbl;
}

/*
 * 8bpp primitives
 */

static void GmgrHLine8(gfx_t *gfx, int x1, int x2, int y, MGLcolour clr)
{
    uint8_t *ptr, *dtbl;

    dtbl = Bpp8PrepareDitherTable(clr);
    for (ptr = video_base + x1 + y * gmgr_mode.bytesPerLine;
        x1 < x2;
        x1++, ptr++)
        *ptr = dtbl[(y % 8) * 8 + x1 % 8];
}

static void GmgrVLine8(gfx_t *gfx, int x, int y1, int y2, MGLcolour clr)
{
    uint8_t *ptr, *dtbl;

    dtbl = Bpp8PrepareDitherTable(clr);
    for (ptr = video_base + x + y1 * gmgr_mode.bytesPerLine;
        y1 < y2;
        y1++, ptr += gmgr_mode.bytesPerLine)
        *ptr = dtbl[(y1 % 8) * 8 + x % 8];
}

static void GmgrPutPixel8(gfx_t *gfx, int x, int y, MGLcolour clr)
{
	uint8_t *dtbl;
    dtbl = Bpp8PrepareDitherTable(clr);
	video_base[x + y * gmgr_mode.bytesPerLine] = dtbl[(y % 8) * 8 + x % 8];
}

static MGLcolour GmgrGetPixel8(gfx_t *gfx, int x, int y)
{
    uint8_t pix;

    pix = video_base[x + y * gmgr_mode.bytesPerLine];
    return MGL_COLOUR(bpp8_palette[pix].red,
        bpp8_palette[pix].green,
        bpp8_palette[pix].blue);
}

static const gfx_vtbl_t gfx_vtbl_8 =
{
    NULL,           /* flush */
	GmgrLineDefault,
	GmgrHLine8,
	GmgrVLine8,
	GmgrPutPixel8,
	GmgrGetPixel8,
    NULL,           /* fill_rect */
};

const gfx_vtbl_t *GmgrInit8(void)
{
    Bpp8GeneratePalette(bpp8_palette);
    return &gfx_vtbl_8;
}
