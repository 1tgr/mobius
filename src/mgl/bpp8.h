/* $Id: bpp8.h,v 1.1 2002/09/13 23:23:01 pavlovskii Exp $ */

#ifndef BPP8_H__
#define BPP8_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <mgl/types.h>
#include <os/video.h>

extern rgb_t bpp8_palette[256];
uint8_t bpp8Dither(int x, int y, MGLcolour clr);
void    bpp8GeneratePalette(rgb_t *palette);

#ifdef __cplusplus
}
#endif

#endif
