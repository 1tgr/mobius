/* $Id: types.h,v 1.2 2002/04/20 12:34:38 pavlovskii Exp $ */

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

#endif
};

typedef struct MGLclip MGLclip;
struct MGLclip
{
    unsigned num_rects;
    MGLrect *rects;
};

#endif
