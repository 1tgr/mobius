/* $Id: bpp8.h,v 1.2 2002/12/18 23:06:10 pavlovskii Exp $ */

#ifndef BPP8_H__
#define BPP8_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <mgl/types.h>
#include <os/video.h>

extern rgb_t bpp8_palette[256];
uint8_t Bpp8Dither(int x, int y, MGLcolour clr);
void    Bpp8GeneratePalette(rgb_t *palette);
uint8_t *Bpp8PrepareDitherTable(MGLcolour clr);

#ifdef __cplusplus
}
#endif

#endif
