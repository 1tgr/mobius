/* $Id$ */

#include "internal.h"

#include <stddef.h>

bool RectTestOverlap(const rect_t *a, const rect_t *b)
{
	if (a->right < b->left)
		return false;
	if (b->right < a->left)
		return false;
	if (a->bottom < b->top)
		return false;
	if (b->bottom < a->top)
		return false;
	return true;
}


bool RectIsEmpty(const rect_t *r)
{
    return r->right - r->left == 0 &&
        r->bottom - r->top == 0;
}


void RectOffset(rect_t *r, int dx, int dy)
{
	r->left += dx;
	r->top += dy;
	r->right += dx;
	r->bottom += dy;
}


void RectUnion(rect_t *dest, const rect_t *a, const rect_t *b)
{
    if (RectIsEmpty(a))
        *dest = *b;
    else if (RectIsEmpty(b))
        *dest = *a;
    else
    {
        dest->left = min(a->left, b->left);
        dest->top = min(a->top, b->top);
        dest->right = max(a->right, b->right);
        dest->bottom = max(a->bottom, b->bottom);
    }
}
