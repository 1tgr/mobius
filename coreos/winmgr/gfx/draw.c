/*
 * $History: draw.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:45
 * Updated in $/coreos/winmgr/gfx
 * Added call to GmgrUnlockGraphics on failure in GmgrDrawText
 * Added history block
 */

#include "internal.h"

#include <stddef.h>

static void swap(int *a, int *b)
{
    int t;
    t = *a;
    *a = *b;
    *b = t;
}


void GmgrHLine(gfx_t *gfx, int x1, int x2, int y, colour_t clr)
{
    unsigned i;
    int ax1, ax2;
    int sx1, sx2, sy;

    if (x2 < x1)
        swap(&x1, &x2);

    for (i = 0; i < gfx->clip.num_rects; i++)
    {
        ax1 = max(x1, gfx->clip.rects[i].left);
        ax2 = max(x2, gfx->clip.rects[i].right);
        GmgrToScreenPoint(&sx1, &sy, ax1, y);
        GmgrToScreenPoint(&sx2, &sy, ax1, y);

        if (ax2 > ax1 &&
            y >= gfx->clip.rects[i].top && y < gfx->clip.rects[i].bottom)
            gfx->surface->surf.vtbl->SurfHLine(&gfx->surface->surf, sx1, sx2, sy, clr);
    }
}


void GmgrVLine(gfx_t *gfx, int x, int y1, int y2, colour_t clr)
{
    unsigned i;
    int ay1, ay2;
    int sx, sy1, sy2;

    if (y2 < y1)
        swap(&y1, &y2);

    for (i = 0; i < gfx->clip.num_rects; i++)
    {
        if (x >= gfx->clip.rects[i].left && x < gfx->clip.rects[i].right)
        {
            ay1 = max(gfx->clip.rects[i].top, y1);
            ay2 = min(gfx->clip.rects[i].bottom, y2);
            GmgrToScreenPoint(&sx, &sy1, x, ay1);
            GmgrToScreenPoint(&sx, &sy2, x, ay2);

            gfx->surface->surf.vtbl->SurfVLine(&gfx->surface->surf, sx, sy1, sy2, clr);
        }
    }
}


void GmgrPutPixel(gfx_t *gfx, int x, int y, colour_t clr)
{
    unsigned i;
    int ax, ay;

    GmgrToScreenPoint(&ax, &ay, x, y);
    for (i = 0; i < gfx->clip.num_rects; i++)
        if (ax >= gfx->clip.rects[i].left &&
            ay >= gfx->clip.rects[i].top &&
            ax <  gfx->clip.rects[i].right &&
            ay <  gfx->clip.rects[i].bottom)
        {
            gfx->surface->surf.vtbl->SurfPutPixel(&gfx->surface->surf, ax, ay, clr);
            break;
        }
}


colour_t GmgrGetPixel(gfx_t *gfx, int x, int y)
{
    int ax, ay;

    GmgrToScreenPoint(&ax, &ay, x, y);
    return gfx->surface->surf.vtbl->SurfGetPixel(&gfx->surface->surf, ax, ay);
}


void GmgrFillRect(gfx_h hgfx, const rect_t *rect)
{
    gfx_t *gfx;
    rect_t r, clip;
    int ax1, ay1, ax2, ay2;
    unsigned i;

    gfx = GmgrLockGraphics(hgfx);
    if (gfx == NULL)
        return;

    GmgrToScreen(&r, rect);

    for (i = 0; i < gfx->clip.num_rects; i++)
    {
        GmgrToScreen(&clip, gfx->clip.rects + i);
        ay1 = max(r.top, clip.top);
        ay2 = min(r.bottom, clip.bottom);

        if (ay2 > ay1)
        {
            ax1 = max(clip.left, r.left);
            ax2 = min(clip.right, r.right);

            if (ax2 > ax1)
                gfx->surface->surf.vtbl->SurfFillRect(&gfx->surface->surf, 
                    ax1,
                    ay1,
                    ax2,
                    ay2,
                    gfx->colour_fill);
        }
    }

    GmgrUnlockGraphics(gfx);
}


/* From NewOS: GraphicsContext.cpp */

enum
{
    kBottom = 1,
    kTop = 2,
    kLeft = 4,
    kRight = 8
};


inline unsigned clipmask(int x, int y, const rect_t *rect)
{
    unsigned mask = 0;
    if (x < rect->left)
        mask |= kLeft;
    else if (x > rect->right)
        mask |= kRight;

    if (y < rect->top)
        mask |= kTop;
    else if (y > rect->bottom)
        mask |= kBottom;

    return mask;
}


inline int vert_intersection(int x1, int y1, int x2, int y2, int x)
{
    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}


inline int horz_intersection(int x1, int y1, int x2, int y2, int y)
{
    return x1 + (x2 - x1) * (y - y1) / (y2 - y1);
}


void GmgrLine(gfx_h hgfx, int x1, int y1, int x2, int y2)
{
    gfx_t *gfx;
    int ax1, ay1, ax2, ay2;
    unsigned i, point1mask, point2mask, mask;
    int clippedX1, clippedY1, clippedX2, clippedY2, x, y;
    bool rejected;

    gfx = GmgrLockGraphics(hgfx);
    if (gfx == NULL)
        return;

    for (i = 0; i < gfx->clip.num_rects; i++)
    {
        clippedX1 = x1;
        clippedY1 = y1;
        clippedX2 = x2;
        clippedY2 = y2;
        point1mask = clipmask(clippedX1, clippedY1, gfx->clip.rects + i);
        point2mask = clipmask(clippedX2, clippedY2, gfx->clip.rects + i);

        rejected = false;
        while (point1mask != 0 || point2mask != 0)
        {
            if ((point1mask & point2mask) != 0)
            {
                rejected = true;
                break;
            }

            mask = point1mask ? point1mask : point2mask;
            x = y = 0;
            if (mask & kBottom)
            {
                y = gfx->clip.rects[i].bottom;
                x = horz_intersection(clippedX1, clippedY1, clippedX2, clippedY2, y);
            }
            else if (mask & kTop)
            {
                y = gfx->clip.rects[i].top;
                x = horz_intersection(clippedX1, clippedY1, clippedX2, clippedY2, y);
            }
            else if (mask & kRight)
            {
                x = gfx->clip.rects[i].right;
                y = vert_intersection(clippedX1, clippedY1, clippedX2, clippedY2, x);
            }
            else if (mask & kLeft)
            {
                x = gfx->clip.rects[i].left;
                y = vert_intersection(clippedX1, clippedY1, clippedX2, clippedY2, x);
            }

            if (point1mask)
            {
                // Clip point 1
                point1mask = clipmask(x, y, gfx->clip.rects + i);
                clippedX1 = x;
                clippedY1 = y;
            }
            else
            {
                // Clip point 2
                point2mask = clipmask(x, y, gfx->clip.rects + i);
                clippedX2 = x;
                clippedY2 = y;
            }
        }

        if (!rejected)
        {
            GmgrToScreenPoint(&ax1, &ay1, clippedX1, clippedY1);
            GmgrToScreenPoint(&ax2, &ay2, clippedX2, clippedY2);
            gfx->surface->surf.vtbl->SurfLine(&gfx->surface->surf, 
                ax1, 
                ay1, 
                ax2, 
                ay2, 
                gfx->colour_pen);
        }
    }

    GmgrUnlockGraphics(gfx);
}


void GmgrRectangle(gfx_h hgfx, const rect_t *rect)
{
    GmgrLine(hgfx, rect->left,        rect->top,            rect->right - 1,    rect->top);
    GmgrLine(hgfx, rect->right - 1,    rect->top,            rect->right - 1,    rect->bottom - 1);
    GmgrLine(hgfx, rect->right - 1,    rect->bottom - 1,    rect->left,            rect->bottom - 1);
    GmgrLine(hgfx, rect->left,        rect->bottom - 1,    rect->left,            rect->top);
}


void GmgrBlt(gfx_h hdest, const rect_t *dest, gfx_h hsrc, const rect_t *src)
{
    gfx_t *gsrc, *gdest;
    gmgr_surface_t *ssrc, *sdest;
    rect_t rsrc, rdest;

    gsrc = GmgrLockGraphics(hsrc);
    if (gsrc == NULL)
        return;

    gdest = GmgrLockGraphics(hdest);
    if (gdest == NULL)
    {
        GmgrUnlockGraphics(gsrc);
        return;
    }

    GmgrToScreen(&rsrc, src);
    GmgrToScreen(&rdest, dest);

    ssrc = gsrc->surface;
    sdest = gdest->surface;

    if (ssrc->device == NULL && sdest->device == NULL)
    {
        /* xxx -- memory-to-memory copy */
    }
    else if (ssrc->device == NULL && sdest->device != NULL)
    {
        /* xxx -- memory-to-screen blt */
        char *src;
        src = (char*) ssrc->base 
            + rsrc.top * ssrc->mode.bytesPerLine
            + (rsrc.left * ssrc->mode.bitsPerPixel) / 8;
        sdest->surf.vtbl->SurfBltMemoryToScreen(&sdest->surf, 
            &rdest, 
            src, 
            ssrc->mode.bytesPerLine);
    }
    else if (ssrc->device != NULL && sdest->device == NULL)
    {
        /* xxx -- screen-to-memory blt */
    }
    else if (ssrc->device != NULL && sdest->device != NULL)
    {
        /* xxx -- screen-to-screen blt */
    }

    GmgrUnlockGraphics(gsrc);
    GmgrUnlockGraphics(gdest);
}


void GmgrDrawText(gfx_h hgfx, const rect_t *rect, const wchar_t *str, size_t length)
{
    point_t size;
    videomode_t mode;
    gmgr_surface_t *surface;
    gfx_t *gfx;
    rect_t clip;
    int ax1, ay1, ax2, ay2;
    unsigned i;

    gfx = GmgrLockGraphics(hgfx);
    if (gfx == NULL)
        return;

    FontGetTextSize(gmgr_font, str, length, &size);
    if (size.x < rect->right - rect->left)
        size.x = rect->right - rect->left;
    if (size.y < rect->bottom - rect->top)
        size.y = rect->bottom - rect->top;

    mode = gmgr_screen->mode;
    mode.width = size.x;
    mode.height = size.y;
    mode.bytesPerLine = (mode.bitsPerPixel * size.x) / 8;
    surface = GmgrCreateMemorySurface(&mode, GMGR_SURFACE_SHARED);
    if (surface == NULL)
    {
        GmgrUnlockGraphics(gfx);
        return;
    }

    FontDrawText(gmgr_font, &surface->surf, 0, 0, str, length, 
        gfx->colour_pen, gfx->colour_fill);

    for (i = 0; i < gfx->clip.num_rects; i++)
    {
        clip = gfx->clip.rects[i];
        ay1 = max(rect->top, clip.top);
        ay2 = min(rect->bottom, clip.bottom);

        if (ay2 > ay1)
        {
            ax1 = max(clip.left, rect->left);
            ax2 = min(clip.right, rect->right);

            if (ax2 > ax1)
            {
                rect_t dest;
                int src_x, src_y;

                dest.left = ax1;
                dest.top = ay1;
                dest.right = ax2;
                dest.bottom = ay2;
                src_x = dest.left - rect->left;
                src_y = dest.top - rect->top;

                gfx->surface->surf.vtbl->SurfBltMemoryToScreen(&gfx->surface->surf,
                    &dest,
                    (uint8_t*) surface->base 
                        + src_y * surface->mode.bytesPerLine
                        + (src_x * surface->mode.bitsPerPixel) / 8,
                    surface->mode.bytesPerLine);
            }
        }
    }

    GmgrCloseSurface(surface);
    GmgrUnlockGraphics(gfx);
}


void GmgrGetTextSize(gfx_h hgfx, const wchar_t *str, size_t length, point_t *size)
{
    FontGetTextSize(gmgr_font, str, length, size);
}


void GmgrFinishedPainting(gfx_h hgfx)
{
    gfx_t *gfx;

    gfx = GmgrLockGraphics(hgfx);
    if (gfx == NULL)
        return;

    gfx->surface->surf.vtbl->SurfFlush(&gfx->surface->surf);

    GmgrUnlockGraphics(gfx);
}


