/*
 * $History: bpp8.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:31
 * Updated in $/coreos/framebuf
 * Tidied up
 * Added history block
 */

#include <os/framebuf.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

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


static void Bpp8DeleteCookie(surface_t *surf)
{
    free(surf->cookie);
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

    ptr = b8surf->base + x1 + y * b8surf->mode.bytesPerLine;
    while (x1 < x2)
    {
        *ptr = Bpp8Dither(x1, y, clr);
        x1++;
        ptr++;
    }
}


void Bpp8BltScreenToScreen(surface_t *surf, const rect_t *dest, const rect_t *src)
{
    bpp8_surf_t *b8surf;
    const uint8_t *src_ptr;
    uint8_t *dest_ptr;
    unsigned y;

    b8surf = surf->cookie;
    src_ptr = b8surf->base + src->top * b8surf->mode.bytesPerLine + src->left;
    dest_ptr = b8surf->base + dest->top * b8surf->mode.bytesPerLine + dest->left;

    for (y = dest->top; y < dest->bottom; y++)
    {
        memcpy(dest_ptr, src_ptr, dest->right - dest->left);
        src_ptr += b8surf->mode.bytesPerLine;
        dest_ptr += b8surf->mode.bytesPerLine;
    }
}


void Bpp8BltScreenToMemory(surface_t *surf, void *dest, int dest_pitch, const rect_t *src)
{
    bpp8_surf_t *b8surf;
    const uint8_t *src_ptr;
    uint8_t *dest_ptr;
    unsigned y;

    b8surf = surf->cookie;
    src_ptr = b8surf->base + src->top * b8surf->mode.bytesPerLine + src->left;
    dest_ptr = dest;

    for (y = src->top; y < src->bottom; y++)
    {
        memcpy(dest_ptr, src_ptr, src->right - src->left);
        src_ptr += b8surf->mode.bytesPerLine;
        dest_ptr += dest_pitch;
    }
}


void Bpp8BltMemoryToScreen(surface_t *surf, const rect_t *dest, const void *src, int src_pitch)
{
    bpp8_surf_t *b8surf;
    const uint8_t *src_ptr;
    uint8_t *dest_ptr;
    unsigned y;

    b8surf = surf->cookie;
    src_ptr = src;
    dest_ptr = b8surf->base + dest->top * b8surf->mode.bytesPerLine + dest->left;

    for (y = dest->top; y < dest->bottom; y++)
    {
        memcpy(dest_ptr, src_ptr, dest->right - dest->left);
        src_ptr += src_pitch;
        dest_ptr += b8surf->mode.bytesPerLine;
    }
}


void Bpp8BltMemoryToMemory(surface_t *surf, 
                           bitmapdesc_t *dest, const rect_t *dest_rect,
                           const bitmapdesc_t *src, const rect_t *src_rect)
{
    const uint8_t *src_ptr;
    uint8_t *dest_ptr;
    unsigned y;

    src_ptr = (uint8_t*) src->data + src_rect->top * src->pitch + src_rect->left;
    dest_ptr = (uint8_t*) dest->data + dest_rect->top * dest->pitch + dest_rect->left;

    for (y = dest_rect->top; y < dest_rect->bottom; y++)
    {
        memcpy(dest_ptr, src_ptr, dest_rect->right - dest_rect->left);
        src_ptr += src->pitch;
        dest_ptr += dest->pitch;
    }
}


static status_t Bpp8ConvertBitmap1(void *dest, int dest_pitch, 
                                   const bitmapdesc_t *src)
{
    const uint8_t *src_ptr, *src_base;
    uint8_t *dest_base;
    uint8_t src_mask;
    colour_t fg, bg;
    unsigned x, y;

    bg = MAKE_COLOUR(
        src->palette[0].red, 
        src->palette[0].green, 
        src->palette[0].blue);
    fg = MAKE_COLOUR(
        src->palette[1].red, 
        src->palette[1].green, 
        src->palette[1].blue);

    src_base = src->data;
    dest_base = dest;
    for (y = 0; y < src->height; y++)
    {
        src_ptr = src_base;
        src_mask = 0x80;

        for (x = 0; x < src->width; x++)
        {
            if ((*src_ptr & src_mask) == 0)
                dest_base[x] = Bpp8Dither(x, y, bg);
            else
                dest_base[x] = Bpp8Dither(x, y, fg);

            src_mask >>= 1;
            if (src_mask == 0)
            {
                src_mask = 0x80;
                src_ptr++;
            }
        }

        src_base += src->pitch;
        dest_base += dest_pitch;
    }

    return 0;
}


static status_t Bpp8ConvertBitmap8(void *dest, int dest_pitch, 
                                   const bitmapdesc_t *src)
{
    const uint8_t *src_base;
    uint8_t *dest_base;
    unsigned x, y;

    src_base = src->data;
    dest_base = dest;
    for (y = 0; y < src->height; y++)
    {
        for (x = 0; x < src->width; x++)
            dest_base[x] = Bpp8Dither(x, y, 
                MAKE_COLOUR(src->palette[src_base[x]].red, 
                    src->palette[src_base[x]].green, 
                    src->palette[src_base[x]].blue));

        src_base += src->pitch;
        dest_base += dest_pitch;
    }

    return 0;
}


static bool Bpp8InitBitmap(void **bitmap, int *pitch, 
                           unsigned width, unsigned height)
{
    *pitch = width;
    *bitmap = malloc(*pitch * height);
    return bitmap != NULL;
}


status_t Bpp8ConvertBitmap(surface_t *surf, 
                           void **dest, int *dest_pitch, 
                           const bitmapdesc_t *src)
{
    if (!Bpp8InitBitmap(dest, dest_pitch, src->width, src->height))
        return errno;

    if (src->bits_per_pixel == 0)
    {
        if (src->palette != NULL)
        {
            uint8_t *dest_base;
            unsigned x, y;

            dest_base = *dest;
            for (y = 0; y < src->height; y++)
            {
                for (x = 0; x < src->width; x++)
                {
                    dest_base[x] = Bpp8Dither(x, y, 
                        MAKE_COLOUR(
                            src->palette[0].red, 
                            src->palette[0].green, 
                            src->palette[0].blue));
                }

                dest_base += *dest_pitch;
            }
        }

        return 0;
    }

    switch (src->bits_per_pixel)
    {
    case 1:
        return Bpp8ConvertBitmap1(*dest, *dest_pitch, src);

    case 8:
        return Bpp8ConvertBitmap8(*dest, *dest_pitch, src);

    default:
        free(*dest);
        *dest = NULL;
        return ENOTIMPL;
    }
}


static const surface_vtbl_t bpp8_vtbl =
{
    Bpp8DeleteCookie,
    Bpp8PutPixel,
    Bpp8GetPixel,
    Bpp8HLine,
    SurfVLine,
    SurfLine,
    SurfFillRect,
    Bpp8BltScreenToScreen,
    Bpp8BltScreenToMemory,
    Bpp8BltMemoryToScreen,
    Bpp8BltMemoryToMemory,
    SurfFlush,
    Bpp8ConvertBitmap,
};


bool Bpp8CreateSurface(const videomode_t *mode, void *base, surface_t *surf)
{
    bpp8_surf_t *b8surf;

    b8surf = malloc(sizeof(*b8surf));
    if (b8surf == NULL)
        return false;

    b8surf->mode = *mode;
    b8surf->base = base;

    surf->vtbl = &bpp8_vtbl;
    surf->cookie = b8surf;
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
