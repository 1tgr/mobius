/*
 * $History: surface.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:32
 * Updated in $/coreos/framebuf
 * Added call to Bpp4CreateSurface
 * Added history block
 */

#include <os/framebuf.h>

bool Bpp4CreateSurface(const videomode_t *mode, void *base, surface_t *surf);
bool Bpp8CreateSurface(const videomode_t *mode, void *base, surface_t *surf);


void SurfFlush(surface_t *surf)
{
}


bool FramebufCreateSurface(const videomode_t *mode, void *base, surface_t *surf)
{
    switch (mode->bitsPerPixel)
    {
    case 4:
        return Bpp4CreateSurface(mode, base, surf);

    case 8:
        return Bpp8CreateSurface(mode, base, surf);
    }

    return false;
}
