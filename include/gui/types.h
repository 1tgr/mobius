// $Id$
#ifndef __GUI_TYPES_H
#define __GUI_TYPES_H

#include <os/video.h>

namespace OS
{

struct Rect : rect_t
{
    Rect()
    {
    }

    Rect(int _left, int _top, int _right, int _bottom)
    {
        left = _left;
        top = _top;
        right = _right;
        bottom = _bottom;
    }

	Rect(const rect_t& rect)
	{
		left = rect.left;
		top = rect.top;
		right = rect.right;
		bottom = rect.bottom;
	}

    int Width() const
    {
        return right - left;
    }

    int Height() const
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

    bool IncludesPoint(int x, int y) const
    {
        return x >= left && y >= top &&
            x < right && y < bottom;
    }
};


struct Point : point_t
{
	Point()
	{
	}

	Point(int _x, int _y)
	{
		x = _x;
		y = _y;
	}

	Point(const point_t& pt)
	{
		x = pt.x;
		y = pt.y;
	}
};

};

#endif
