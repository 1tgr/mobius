/*
 * $History: accel.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:30
 * Updated in $/coreos/framebuf
 * Stopped closing device handle in AccelDeleteCookie
 * Added history block
 */

#include <os/framebuf.h>

#ifdef KERNEL
#include <kernel/kernel.h>
#endif

#include <os/queue.h>
#include <os/rtl.h>
#include <os/syscall.h>

#include <stdlib.h>
#include <errno.h>

typedef struct accel_surf_t accel_surf_t;
struct accel_surf_t
{
    handle_t device;
    queue_t commands;
    unsigned bits_per_pixel;
};


static void AccelFlush(surface_t *surf)
{
    accel_surf_t *asurf;
    params_vid_t params;

    asurf = surf->cookie;
    if (asurf->commands.length  > 0)
    {
        params.vid_draw.shapes = QUEUE_DATA(asurf->commands);
        params.vid_draw.length = asurf->commands.length;
        FsRequestSync(asurf->device, VID_DRAW, &params, sizeof(params), NULL);
        QueueClear(&asurf->commands);
    }
}


void AccelDeleteCookie(surface_t *surf)
{
    accel_surf_t *asurf;

    asurf = surf->cookie;
    AccelFlush(surf);
}


static void AccelPutPixel(surface_t *surf, int x, int y, colour_t clr)
{
    accel_surf_t *asurf;
    vid_shape_t shape;

    asurf = surf->cookie;
    shape.shape = VID_SHAPE_PUTPIXEL;
    shape.s.pix.colour = clr;
    shape.s.pix.point.x = x;
    shape.s.pix.point.y = y;
    QueueAppend(&asurf->commands, &shape, sizeof(shape));
}


static colour_t AccelGetPixel(surface_t *surf, int x, int y)
{
    accel_surf_t *asurf;
    vid_shape_t shape;
    colour_t ret;
    params_vid_t params;

    asurf = surf->cookie;
    shape.shape = VID_SHAPE_GETPIXEL;
    shape.s.pix.point.x = x;
    shape.s.pix.point.y = y;
    QueueAppend(&asurf->commands, &shape, sizeof(shape));

    params.vid_draw.shapes = QUEUE_DATA(asurf->commands);
    params.vid_draw.length = asurf->commands.length;
    FsRequestSync(asurf->device, VID_DRAW, &params, sizeof(params), NULL);
    ret = ((vid_shape_t*) ((char*) QUEUE_DATA(asurf->commands) + asurf->commands.length))[-1].s.pix.colour;

    QueueClear(&asurf->commands);
    return ret;
}


static void AccelHLine(surface_t *surf, int x1, int x2, int y, colour_t clr)
{
    accel_surf_t *asurf;
    vid_shape_t shape;

    asurf = surf->cookie;
    shape.shape = VID_SHAPE_HLINE;
    shape.s.line.a.x = x1;
    shape.s.line.a.y = y;
    shape.s.line.b.x = x2;
    shape.s.line.b.y = y;
    shape.s.line.colour = clr;
    QueueAppend(&asurf->commands, &shape, sizeof(shape));
}


static void AccelVLine(surface_t *surf, int x, int y1, int y2, colour_t clr)
{
    accel_surf_t *asurf;
    vid_shape_t shape;

    asurf = surf->cookie;
    shape.shape = VID_SHAPE_VLINE;
    shape.s.line.a.x = x;
    shape.s.line.a.y = y1;
    shape.s.line.b.x = x;
    shape.s.line.b.y = y2;
    shape.s.line.colour = clr;
    QueueAppend(&asurf->commands, &shape, sizeof(shape));
}


static void AccelLine(surface_t *surf, int x1, int y1, int x2, int y2, colour_t clr)
{
    accel_surf_t *asurf;
    vid_shape_t shape;

    asurf = surf->cookie;
    shape.shape = VID_SHAPE_LINE;
    shape.s.line.a.x = x1;
    shape.s.line.a.y = y1;
    shape.s.line.b.x = x2;
    shape.s.line.b.y = y2;
    shape.s.line.colour = clr;
    QueueAppend(&asurf->commands, &shape, sizeof(shape));
}


static void AccelFillRect(surface_t *surf, int x1, int y1, int x2, int y2, colour_t clr)
{
    accel_surf_t *asurf;
    vid_shape_t shape;

    asurf = surf->cookie;
    shape.shape = VID_SHAPE_FILLRECT;
    shape.s.rect.colour = clr;
    shape.s.rect.rect.left = x1;
    shape.s.rect.rect.top = y1;
    shape.s.rect.rect.right = x2;
    shape.s.rect.rect.bottom = y2;
    QueueAppend(&asurf->commands, &shape, sizeof(shape));
}


static void AccelBltScreenToScreen(surface_t *surf, const rect_t *dest, const rect_t *src)
{
    accel_surf_t *asurf;
    params_vid_t params;

    asurf = surf->cookie;
    AccelFlush(surf);
    params.vid_blt.dest.rect = *dest;
    params.vid_blt.src.rect = *src;
    FsRequestSync(asurf->device, VID_BLT_SCREEN_TO_SCREEN, &params, sizeof(params), NULL);
}


static void AccelBltScreenToMemory(surface_t *surf, void *dest, int dest_pitch, const rect_t *src)
{
    accel_surf_t *asurf;
    params_vid_t params;

    asurf = surf->cookie;
    AccelFlush(surf);
    params.vid_blt.dest.memory.buffer = dest;
    params.vid_blt.dest.memory.pitch = dest_pitch;
    params.vid_blt.src.rect = *src;
    FsRequestSync(asurf->device, VID_BLT_SCREEN_TO_MEMORY, &params, sizeof(params), NULL);
}


static void AccelBltMemoryToScreen(surface_t *surf, const rect_t *dest, const void *src, int src_pitch)
{
    accel_surf_t *asurf;
    params_vid_t params;

    asurf = surf->cookie;
    AccelFlush(surf);
    params.vid_blt.dest.rect = *dest;
    params.vid_blt.src.memory.buffer = (void*) src;
    params.vid_blt.src.memory.pitch = src_pitch;
    FsRequestSync(asurf->device, VID_BLT_MEMORY_TO_SCREEN, &params, sizeof(params), NULL);
}


static void AccelBltMemoryToMemory(surface_t *surf, 
                                   bitmapdesc_t *dest, const rect_t *dest_rect,
                                   const bitmapdesc_t *src, const rect_t *src_rect)
{
    accel_surf_t *asurf;

    asurf = surf->cookie;
    switch (asurf->bits_per_pixel)
    {
    case 4:
        return Bpp4BltMemoryToMemory(surf, dest, dest_rect, src, src_rect);

    case 8:
        return Bpp8BltMemoryToMemory(surf, dest, dest_rect, src, src_rect);
    }
}


static status_t AccelConvertBitmap(surface_t *surf, 
                                   void **dest, int *dest_pitch, 
                                   const bitmapdesc_t *src)
{
    accel_surf_t *asurf;

    asurf = surf->cookie;
    switch (asurf->bits_per_pixel)
    {
    case 4:
        return Bpp4ConvertBitmap(surf, dest, dest_pitch, src);

    case 8:
        return Bpp8ConvertBitmap(surf, dest, dest_pitch, src);

    default:
        return ENOTIMPL;
    }
}


static const surface_vtbl_t accel_vtbl =
{
    AccelDeleteCookie,
    AccelPutPixel,
    AccelGetPixel,
    AccelHLine,
    AccelVLine,
    AccelLine,
    AccelFillRect,
    AccelBltScreenToScreen,
    AccelBltScreenToMemory,
    AccelBltMemoryToScreen,
    AccelBltMemoryToMemory,
    AccelFlush,
    AccelConvertBitmap,
};


bool AccelCreateSurface(const videomode_t *mode, handle_t device, surface_t *surf)
{
    accel_surf_t *asurf;

    asurf = malloc(sizeof(*asurf));
    if (asurf == NULL)
        return false;

    asurf->device = device;
    QueueInit(&asurf->commands);
    asurf->bits_per_pixel = mode->bitsPerPixel;

    surf->vtbl = &accel_vtbl;
    surf->cookie = asurf;
    return true;
}
