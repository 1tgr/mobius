/* $Id: vidfuncs.h,v 1.2 2003/06/05 21:59:53 pavlovskii Exp $ */

#ifndef VIDFUNCS_H__
#define VIDFUNCS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "video.h"

void vidFillPolygon(video_t *vid, const point_t *points, unsigned vertices, 
                    colour_t colour);
void vidFillRect(video_t *vid, int x1, int y1, int x2, int y2, colour_t c);
void vidHLine(video_t *vid, int x1, int x2, int y, colour_t c);
void vidVLine(video_t *vid, int x, int y1, int y2, colour_t c);
void vidLine(video_t *vid, int x1, int y1, int x2, int y2, colour_t d);
//void vidTextOut(video_t *vid, rect_t *rect, 
                //const wchar_t *str, size_t len, colour_t fg, colour_t bg);
//bool vidInitText(void);

#ifdef __cplusplus
}
#endif

#endif
