/* $Id: types.h,v 1.3 2002/08/04 17:22:39 pavlovskii Exp $ */

#ifndef __GL_TYPES_H
#define __GL_TYPES_H

#include <sys/types.h>

typedef    int	    MGLreal;
typedef    uint32_t MGLcolour;
#define    MGLcolor MGLcolour

typedef struct mglrc_t mglrc_t;

typedef struct MGLpoint MGLpoint;
/*! \brief  MGL point */
/*!
 *  Specifies x and y coordinates
 */
struct MGLpoint
{
    MGLreal x, y;

#ifdef __cplusplus
    MGLpoint()
    {
    }

    MGLpoint(MGLreal _x, MGLreal _y)
    {
        x = _x;
        y = _y;
    }
#endif
};

typedef struct MGLrect MGLrect;
/*! \brief  MGL rectangle */
struct MGLrect
{
    MGLreal left, top, right, bottom;

#ifdef __cplusplus
    MGLrect()
    {
    }

    MGLrect(MGLreal _left, MGLreal _top, MGLreal _right, MGLreal _bottom)
    {
        left = _left;
        top = _top;
        right = _right;
        bottom = _bottom;
    }

    MGLreal Width() const
    {
        return right - left;
    }

    MGLreal Height() const
    {
        return bottom - top;
    }

    void Inflate(int dx, int dy)
    {
        left -= dx;
        top -= dy;
        right += dx;
        bottom += dy;
    }

    void Offset(int dx, int dy)
    {
        left += dx;
        top += dy;
        right += dx;
        bottom += dy;
    }
#endif
};

typedef struct MGLclip MGLclip;
struct MGLclip
{
    unsigned num_rects;
    MGLrect *rects;
};

#define MGL_ALPHA(c)	((uint8_t) (((c) & 0xff000000) >> 24))
#define MGL_RED(c)	((uint8_t) (((c) & 0x00ff0000) >> 16))
#define MGL_GREEN(c)	((uint8_t) (((c) & 0x0000ff00) >> 8))
#define MGL_BLUE(c)	((uint8_t) (((c) & 0x000000ff)))
#define MGL_COLOUR4(r,g,b,a)	\
    ((uint8_t) (b) | \
     (uint8_t) (g) << 8 | \
     (uint8_t) (r) << 16 | \
     (uint8_t) (a) << 24)
#define MGL_COLOR4(r,g,b,a)    MGL_COLOUR4(r,g,b,a)
#define MGL_COLOUR(r,g,b)    MGL_COLOUR4(r,g,b,0)
#define MGL_COLOR(r,g,b)    MGL_COLOR4(r,g,b,0)

#endif
