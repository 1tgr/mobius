#ifndef BPP8_H__
#define BPP8_H__

extern rgb_t bpp8_palette[256];
uint8_t bpp8Dither(int x, int y, colour_t clr);
void    bpp8GeneratePalette(rgb_t *palette);

#endif
