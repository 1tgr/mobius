/* $Id: types.h,v 1.2 2003/06/22 15:50:13 pavlovskii Exp $ */

#ifndef __GL_TYPES_H
#define __GL_TYPES_H

#include <sys/types.h>

#ifdef WIN32
#include "common.h"

#ifndef MGL_EXPORT
#ifdef LIBMGL_EXPORTS
#define MGL_EXPORT	__declspec(dllexport)
#else
#define MGL_EXPORT
#endif
#endif
#endif

#ifdef __cplusplus
namespace OS
{
#endif

typedef    int	    MGLreal;
typedef    uint32_t MGLcolour;
#define    MGLcolor MGLcolour

typedef struct mgl_rect_t mgl_rect_t;
struct mgl_rect_t
{
	MGLreal left, top, right, bottom;
};

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

#ifdef __cplusplus
typedef struct MGLrect MGLrect;
/*! \brief  MGL rectangle */
struct MGLrect : mgl_rect_t
{
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

	MGLrect(const mgl_rect_t& rect)
	{
		left = rect.left;
		top = rect.top;
		right = rect.right;
		bottom = rect.bottom;
	}

    MGLreal Width() const
    {
        return right - left;
    }

    MGLreal Height() const
    {
        return bottom - top;
    }

    void Inflate(MGLreal dx, MGLreal dy)
    {
        left -= dx;
        top -= dy;
        right += dx;
        bottom += dy;
    }

    void Offset(MGLreal dx, MGLreal dy)
    {
        left += dx;
        top += dy;
        right += dx;
        bottom += dy;
    }

    bool IncludesPoint(MGLreal x, MGLreal y) const
    {
        return x >= left && y >= top &&
            x < right && y < bottom;
    }
};
#else
typedef mgl_rect_t MGLrect;
#endif

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

#ifdef __cplusplus
}
#endif

#endif
