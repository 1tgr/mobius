#include "bpp8.h"

/* Surprisingly effective 8-bit dithering code from Paul Hsieh (qed@pobox.com) */

rgb_t bpp8_palette[256];

#define DS(v) {((v*49+7)/15+1)*(0x010001), ((v*49+7)/15+1)*(0x000100)}

static int DitherTable8[4][4][2] = {
    { DS( 0), DS(11), DS( 1), DS(10) },
    { DS(14), DS( 4), DS(15), DS( 5) },
    { DS( 2), DS( 9), DS( 3), DS( 8) },
    { DS(12), DS( 6), DS(13), DS( 7) }
};

uint8_t bpp8Dither(int x, int y, colour_t clr)
{
    int rb, r, g, b;

    if (COLOUR_RED(clr) / 16 == COLOUR_GREEN(clr) / 16 &&
        COLOUR_GREEN(clr) / 16 == COLOUR_BLUE(clr) / 16)
        return COLOUR_RED(clr) / 16;

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

void bpp8GeneratePalette(rgb_t *palette)
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
