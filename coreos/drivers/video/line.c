/* $Id: line.c,v 1.3 2003/06/22 15:43:37 pavlovskii Exp $ */

#include <stdio.h>
#include "vidfuncs.h"

void SurfFillRect(surface_t *surf, int x1, int y1, int x2, int y2, colour_t c)
{
    for (; y1 < y2; y1++)
        surf->vtbl->SurfHLine(surf, x1, x2, y1, c);
}


void SurfHLine(surface_t *surf, int x1, int x2, int y, colour_t c)
{
    for (; x1 < x2; x1++)
        surf->vtbl->SurfPutPixel(surf, x1, y, c);
}


void SurfVLine(surface_t *surf, int x, int y1, int y2, colour_t c)
{
    for (; y1 < y2; y1++)
        surf->vtbl->SurfPutPixel(surf, x, y1, c);
}


/* from Allegro gfx.c */
static void do_line(surface_t *surf, int x1, int y1, int x2, int y2, colour_t d, 
                    void (*proc)(surface_t*, int, int, colour_t))
{
    int dx = x2-x1;
    int dy = y2-y1;
    int i1, i2;
    int x, y;
    int dd;
    
    /* worker macro */
#define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond)    \
    {                                                                    \
        if (d##pri_c == 0) {                                             \
            proc(surf, x1, y1, d);                                       \
            return;                                                      \
        }                                                                \
        \
        i1 = 2 * d##sec_c;                                               \
        dd = i1 - (sec_sign (pri_sign d##pri_c));                        \
        i2 = dd - (sec_sign (pri_sign d##pri_c));                        \
        \
        x = x1;                                                          \
        y = y1;                                                          \
        \
        while (pri_c pri_cond pri_c##2) {                                \
            proc(surf, x, y, d);                                         \
            if (dd sec_cond 0) {                                         \
                sec_c sec_sign##= 1;                                     \
                dd += i2;                                                \
            }                                                            \
            else                                                         \
                dd += i1;                                                \
            \
            pri_c pri_sign##= 1;                                         \
        }                                                                \
    }
    
    if (dx >= 0) {
        if (dy >= 0) {
            if (dx >= dy) {
                /* (x1 <= x2) && (y1 <= y2) && (dx >= dy) */
                DO_LINE(+, x, <=, +, y, >=);
            }
            else {
                /* (x1 <= x2) && (y1 <= y2) && (dx < dy) */
                DO_LINE(+, y, <=, +, x, >=);
            }
        }
        else {
            if (dx >= -dy) {
                /* (x1 <= x2) && (y1 > y2) && (dx >= dy) */
                DO_LINE(+, x, <=, -, y, <=);
            }
            else {
                /* (x1 <= x2) && (y1 > y2) && (dx < dy) */
                DO_LINE(-, y, >=, +, x, >=);
            }
        }
    }
    else {
        if (dy >= 0) {
            if (-dx >= dy) {
                /* (x1 > x2) && (y1 <= y2) && (dx >= dy) */
                DO_LINE(-, x, >=, +, y, >=);
            }
            else {
                /* (x1 > x2) && (y1 <= y2) && (dx < dy) */
                DO_LINE(+, y, <=, -, x, <=);
            }
        }
        else {
            if (-dx >= -dy) {
                /* (x1 > x2) && (y1 > y2) && (dx >= dy) */
                DO_LINE(-, x, >=, -, y, <=);
            }
            else {
                /* (x1 > x2) && (y1 > y2) && (dx < dy) */
                DO_LINE(-, y, >=, -, x, <=);
            }
        }
    }
}


void SurfLine(surface_t *surf, int x1, int y1, int x2, int y2, colour_t d)
{
    if (x1 == x2)
        surf->vtbl->SurfVLine(surf, x1, y1, y2, d);
    else if (y1 == y2)
        surf->vtbl->SurfHLine(surf, x1, x2, y1, d);
    else
        do_line(surf, x1, y1, x2, y2, d, surf->vtbl->SurfPutPixel);
}
