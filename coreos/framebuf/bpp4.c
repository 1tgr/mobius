/*
 * $History: bpp4.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:31
 * Updated in $/coreos/framebuf
 * Removed redundant functions
 * 
 * *****************  Version 1  *****************
 * User: Tim          Date: 13/03/04   Time: 15:32
 * Created in $/coreos/framebuf
 */

#include <os/framebuf.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct bpp4_surf_t bpp4_surf_t;
struct bpp4_surf_t
{
    videomode_t mode;
    uint8_t *base;
};

static const rgb_t bpp4_palette[16] =
{
    { 0x00, 0x00, 0x00, 0x00 }, // 0
    { 0x80, 0x00, 0x00, 0x00 }, // 1
    { 0x00, 0x80, 0x00, 0x00 }, // 2
    { 0x80, 0x80, 0x00, 0x00 }, // 3
    { 0x00, 0x00, 0x80, 0x00 }, // 4
    { 0x80, 0x00, 0x80, 0x00 }, // 5
    { 0x00, 0x80, 0x80, 0x00 }, // 6
    { 0xc0, 0xc0, 0xc0, 0x00 }, // 8
    { 0x80, 0x80, 0x80, 0x00 }, // 7
    { 0xff, 0x00, 0x00, 0x00 }, // 9
    { 0x00, 0xff, 0x00, 0x00 }, // 10
    { 0xff, 0xff, 0x00, 0x00 }, // 11
    { 0x00, 0x00, 0xff, 0x00 }, // 12
    { 0xff, 0x00, 0xff, 0x00 }, // 13
    { 0x00, 0xff, 0xff, 0x00 }, // 14
    { 0xff, 0xff, 0xff, 0x00 }, // 15
};


static uint8_t Bpp4Dither(int x, int y, colour_t colour)
{
    uint8_t red, green, blue, intensity;

    switch (colour)
    {
    case 0x000000: return 0;
    case 0x800000: return 1;
    case 0x008000: return 2;
    case 0x808000: return 3;
    case 0x000080: return 4;
    case 0x800080: return 5;
    case 0x008080: return 6;
    case 0xc0c0c0: return 7;
    case 0x808080: return 8;
    case 0xff0000: return 9;
    case 0x00ff00: return 10;
    case 0xffff00: return 11;
    case 0x0000ff: return 12;
    case 0xff00ff: return 13;
    case 0x00ffff: return 14;
    case 0xffffff: return 15;

    default:
        red = COLOUR_RED(colour) >= 0x80;
        green = COLOUR_GREEN(colour) >= 0x80;
        blue = COLOUR_BLUE(colour) >= 0x80;
        intensity = (COLOUR_RED(colour) > 0xc0) || 
            (COLOUR_GREEN(colour) > 0xc0) || 
            (COLOUR_BLUE(colour) > 0xc0);

        return (intensity << 3) | (blue << 2) | (green << 1) | red;
    }
}


static void Bpp4DeleteCookie(surface_t *surf)
{
    free(surf->cookie);
}


static void Bpp4PutPixel(surface_t *surf, int x, int y, colour_t clr)
{
    bpp4_surf_t *b4surf;
    uint8_t *ptr;
    b4surf = surf->cookie;
    ptr = b4surf->base + x / 2 + y * b4surf->mode.bytesPerLine;
    if (x & 1)
        *ptr = (*ptr & 0xf0) | Bpp4Dither(x, y, clr);
    else
        *ptr = (*ptr & 0x0f) | (Bpp4Dither(x, y, clr) << 4);
}


static colour_t Bpp4GetPixel(surface_t *surf, int x, int y)
{
    bpp4_surf_t *b4surf;
    uint8_t *ptr, index;

    b4surf = surf->cookie;
    ptr = b4surf->base + x / 2 + y * b4surf->mode.bytesPerLine;
    if (x & 1)
        index = *ptr & 0x0f;
    else
        index = (*ptr & 0xf0) >> 4;
    return MAKE_COLOUR(bpp4_palette[index].red, 
        bpp4_palette[index].green, 
        bpp4_palette[index].blue);
}


static void Bpp4HLine(surface_t *surf, int x1, int x2, int y, colour_t clr)
{
    bpp4_surf_t *b4surf;
    uint8_t *ptr, mask;
    unsigned shift;

    b4surf = surf->cookie;
    if (x2 < x1)
    {
        int temp;
        temp = x1;
        x1 = x2;
        x2 = temp;
    }

    if (x1 & 1)
    {
        mask = 0xf0;
        shift = 0;
    }
    else
    {
        mask = 0x0f;
        shift = 4;
    }

    ptr = b4surf->base + x1 + y * b4surf->mode.bytesPerLine;
    while (x1 < x2)
    {
        *ptr = (*ptr & mask) | (Bpp4Dither(x1, y, clr) << shift);
        mask = ~mask;
        shift = 4 - shift;
        x1++;
        ptr += shift / 4;
    }
}


void Bpp4BltScreenToScreen(surface_t *surf, const rect_t *dest, const rect_t *src)
{
    bpp4_surf_t *b4surf;
    const uint8_t *src_ptr;
    uint8_t *dest_ptr;
    unsigned y;

    b4surf = surf->cookie;
    src_ptr = b4surf->base + src->top * b4surf->mode.bytesPerLine + src->left / 2;
    dest_ptr = b4surf->base + dest->top * b4surf->mode.bytesPerLine + dest->left / 2;

    for (y = dest->top; y < dest->bottom; y++)
    {
        memcpy(dest_ptr, src_ptr, dest->right - dest->left);
        src_ptr += b4surf->mode.bytesPerLine;
        dest_ptr += b4surf->mode.bytesPerLine;
    }
}


void Bpp4BltScreenToMemory(surface_t *surf, void *dest, int dest_pitch, const rect_t *src)
{
    bpp4_surf_t *b4surf;
    const uint8_t *src_ptr;
    uint8_t *dest_ptr;
    unsigned y;

    b4surf = surf->cookie;
    src_ptr = b4surf->base + src->top * b4surf->mode.bytesPerLine + src->left / 2;
    dest_ptr = dest;

    for (y = src->top; y < src->bottom; y++)
    {
        memcpy(dest_ptr, src_ptr, (src->right - src->left) / 2);
        src_ptr += b4surf->mode.bytesPerLine;
        dest_ptr += dest_pitch;
    }
}


void Bpp4BltMemoryToScreen(surface_t *surf, const rect_t *dest, const void *src, int src_pitch)
{
    bpp4_surf_t *b4surf;
    const uint8_t *src_ptr;
    uint8_t *dest_ptr;
    unsigned y;

    b4surf = surf->cookie;
    src_ptr = src;
    dest_ptr = b4surf->base + dest->top * b4surf->mode.bytesPerLine + dest->left / 2;

    for (y = dest->top; y < dest->bottom; y++)
    {
        memcpy(dest_ptr, src_ptr, (dest->right - dest->left) / 2);
        src_ptr += src_pitch;
        dest_ptr += b4surf->mode.bytesPerLine;
    }
}


static const surface_vtbl_t bpp4_vtbl =
{
    Bpp4DeleteCookie,
    Bpp4PutPixel,
    Bpp4GetPixel,
    Bpp4HLine,
    SurfVLine,
    SurfLine,
    SurfFillRect,
    Bpp4BltScreenToScreen,
    Bpp4BltScreenToMemory,
    Bpp4BltMemoryToScreen,
    Bpp4BltMemoryToMemory,
    SurfFlush,
    Bpp4ConvertBitmap,
};


bool Bpp4CreateSurface(const videomode_t *mode, void *base, surface_t *surf)
{
    bpp4_surf_t *b4surf;

    b4surf = malloc(sizeof(*b4surf));
    if (b4surf == NULL)
        return false;

    b4surf->mode = *mode;
    b4surf->base = base;

    surf->vtbl = &bpp4_vtbl;
    surf->cookie = b4surf;
    return true;
}


const rgb_t *Bpp4GetPalette()
{
    return bpp4_palette;
}
