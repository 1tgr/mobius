#ifndef __INTERNAL_RENDER_H
#define __INTERNAL_RENDER_H

bool	vidFillRect(addr_t video, point_t topLeft, point_t bottomRight, 
					pixel_t colour);
bool	vidVLine(addr_t video, int x, int y1, int y2, pixel_t colour);
bool	vidHLine(addr_t video, int x1, int x2, int y, pixel_t colour);
bool	vidLine(addr_t video, point_t from, point_t to, pixel_t colour);
bool	vidPutPixel(addr_t video, int x, int y, pixel_t colour);
pixel_t	vidColourMatch(addr_t video, int x, int y, MGLcolour clr);

#endif