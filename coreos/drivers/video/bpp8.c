/* $Id$ */

#include "video.h"
#include "bpp8.h"

typedef struct bpp8_surf_t bpp8_surf_t;
struct bpp8_surf_t
{
	videomode_t mode;
	uint8_t *base;
};


/* Surprisingly effective 8-bit dithering code from Paul Hsieh (qed@pobox.com) */

static rgb_t bpp8_palette[256];

#define DS(v) {((v*49+7)/15+1)*(0x010001), ((v*49+7)/15+1)*(0x000100)}

static int DitherTable8[4][4][2] = {
    { DS( 0), DS(11), DS( 1), DS(10) },
    { DS(14), DS( 4), DS(15), DS( 5) },
    { DS( 2), DS( 9), DS( 3), DS( 8) },
    { DS(12), DS( 6), DS(13), DS( 7) }
};

static uint8_t Bpp8Dither(int x, int y, colour_t clr)
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


static void Bpp8PutPixel(surface_t *surf, int x, int y, colour_t clr)
{
	bpp8_surf_t *b8surf;
	b8surf = surf->cookie;
    b8surf->base[x + y * b8surf->mode.bytesPerLine] = Bpp8Dither(x, y, clr);
}


static colour_t Bpp8GetPixel(surface_t *surf, int x, int y)
{
	bpp8_surf_t *b8surf;
    uint8_t index;

	b8surf = surf->cookie;
    index = b8surf->base[x + y * b8surf->mode.bytesPerLine];
    return MAKE_COLOUR(bpp8_palette[index].red, 
        bpp8_palette[index].green, 
        bpp8_palette[index].blue);
}


static void Bpp8HLine(surface_t *surf, int x1, int x2, int y, colour_t clr)
{
	bpp8_surf_t *b8surf;
    uint8_t *ptr;

	b8surf = surf->cookie;
    if (x2 < x1)
	{
		int temp;
		temp = x1;
		x1 = x2;
		x2 = temp;
	}

    for (ptr = b8surf->base + x1 + y * b8surf->mode.bytesPerLine;
        x1 < x2;
        x1++, ptr++)
        *ptr = Bpp8Dither(x1, y, clr);
}


static const surface_vtbl_t bpp8_vtbl =
{
	Bpp8PutPixel,
	Bpp8GetPixel,
	Bpp8HLine,
	SurfVLine,
	SurfLine,
	SurfFillRect,
	NULL,			/* bltscreentoscreen */
	NULL,			/* bltscreentomemory */
	NULL,			/* bltmemorytoscreen */
};


bool Bpp8CreateSurface(const videomode_t *mode, void *base, 
					   const surface_vtbl_t **vtbl, void **cookie)
{
	bpp8_surf_t *surf;

	surf = malloc(sizeof(*surf));
	if (surf == NULL)
		return false;

	surf->mode = *mode;
	surf->base = base;

	*vtbl = &bpp8_vtbl;
	*cookie = surf;
	return true;
}


const rgb_t *Bpp8GetPalette()
{
	return bpp8_palette;
}


bool Bpp8Init(void)
{
    int i;

#define SC(x) ((((x)%6) * 252) / 5)
    for (i = 0; i < 16; i++)
    {
        bpp8_palette[i].red = i * 16;
        bpp8_palette[i].green = i * 16;
        bpp8_palette[i].blue = i * 16;
    }

    for (; i < 40; i++)
        bpp8_palette[i].red = 
            bpp8_palette[i].green = 
            bpp8_palette[i].blue = 255;

    for (; i < 256; i++)
    {
        bpp8_palette[i].red = SC((i - 40) / 36);
        bpp8_palette[i].green = SC((i - 40) / 6);
        bpp8_palette[i].blue = SC(i - 40);
    }
#undef SC

    return true;
}
