/* $Id: rect.c,v 1.2 2002/03/06 19:31:41 pavlovskii Exp $ */

#include <gl/mgl.h>
#include <stddef.h>

void RectSet(MGLrect* r, MGLreal left, MGLreal top, MGLreal right, MGLreal bottom)
{
	r->left = left;
	r->top = top;
	r->right = right;
	r->bottom = bottom;
}

void RectSetEmpty(MGLrect* r)
{
	r->left = 0;
	r->top = 0;
	r->right = 0;
	r->bottom = 0;
}

bool RectIsEmpty(const MGLrect* r)
{
	return r->left == 0 && 
		r->top == 0 &&
		r->right == 0 &&
		r->bottom == 0;
}

void RectUnion(MGLrect* a, const MGLrect* b)
{
	if (!RectIsEmpty(b))
	{
		if (RectIsEmpty(a))
			*a = *b;
		else
		{
			a->left = min(a->left, b->left);
			a->top = min(a->top, b->top);
			a->right = max(a->right, b->right);
			a->bottom = max(a->bottom, b->bottom);
		}
	}
}

bool RectIncludesPoint(const MGLrect* r, MGLreal x, MGLreal y)
{
	return x >= r->left &&
		y >= r->top &&
		x < r->right &&
		y < r->bottom;
}

void RectOffset(MGLrect* r, MGLreal dx, MGLreal dy)
{
	r->left += dx;
	r->top += dy;
	r->right += dx;
	r->bottom += dy;
}

bool RectIntersects(const MGLrect* r1, const MGLrect* r2)
{
	if (RectIsEmpty(r1) || RectIsEmpty(r2)
		|| (r1->left >= r2->right)
		|| (r1->right <= r2->left)
		|| (r1->top >= r2->bottom)
		|| (r1->bottom <= r2->top))
		return false;
	else
		return true;
}

void RectInflate(MGLrect* r, MGLreal dx, MGLreal dy)
{
	r->left -= dx;
	r->top -= dy;
	r->right += dx;
	r->bottom += dy;
}
