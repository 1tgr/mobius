/* $Id: bpp8.h,v 1.1 2002/12/21 09:49:08 pavlovskii Exp $ */

#ifndef BPP8_H__
#define BPP8_H__

#ifdef __cplusplus
extern "C"
{
#endif

extern rgb_t bpp8_palette[256];
uint8_t bpp8Dither(int x, int y, colour_t clr);
void    bpp8GeneratePalette(rgb_t *palette);

#ifdef __cplusplus
}
#endif

#endif
