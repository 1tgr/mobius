/* $Id: render.c,v 1.7 2002/04/20 12:47:28 pavlovskii Exp $ */

#include "mgl.h"
#include "render.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <stddef.h>

#include <os/syscall.h>

#define vidAddToQueue(q, s)    QueueAppend(q, s, sizeof(*s))

bool vidFlushQueue(queue_t *queue, mglrc_t *rc)
{
    if (queue->length > 0)
    {
	params_vid_t params;
	fileop_t op;
        rect_t temp;

        if (rc->vid_clip.num_rects == 0)
        {
            temp.left = temp.top = 0;
            temp.right = rc->surf_width;
            temp.bottom = rc->surf_height;
            params.vid_draw.clip.num_rects = 1;
            params.vid_draw.clip.rects = &temp;
        }
        else
            params.vid_draw.clip = rc->vid_clip;

	params.vid_draw.shapes = QUEUE_DATA(*queue);
	params.vid_draw.length = queue->length;
	if (FsRequestSync(rc->video, VID_DRAW, &params, sizeof(params), &op))
	    op.result = 0;

	QueueClear(queue);
	
	if (op.result != 0)
	{
	    errno = op.result;
	    return false;
	}
	else
	    return true;
    }
    else
	return true;
}

void swap_MGLreal(MGLreal *a, MGLreal *b)
{
    MGLreal temp = *b;
    *b = *a;
    *a = temp;
}

bool vidFillRect(queue_t *queue, point_t topLeft, point_t bottomRight, 
		 colour_t colour)
{
    vid_shape_t shape;
    shape.shape = VID_SHAPE_FILLRECT;
    shape.s.rect.rect.left = topLeft.x;
    shape.s.rect.rect.top = topLeft.y;
    shape.s.rect.rect.right = bottomRight.x;
    shape.s.rect.rect.bottom = bottomRight.y;
    shape.s.rect.colour = colour;
    return vidAddToQueue(queue, &shape) != NULL;
}

bool vidVLine(queue_t *queue, int x, int y1, int y2, colour_t colour)
{
    vid_shape_t shape;
    shape.shape = VID_SHAPE_VLINE;
    shape.s.line.a.x = shape.s.line.b.x = x;
    shape.s.line.a.y = y1;
    shape.s.line.b.y = y2;
    shape.s.line.colour = colour;
    return vidAddToQueue(queue, &shape) != NULL;
}

bool vidHLine(queue_t *queue, int x1, int x2, int y, colour_t colour)
{
    vid_shape_t shape;
    shape.shape = VID_SHAPE_HLINE;
    shape.s.line.a.x = x1;
    shape.s.line.b.x = x2;
    shape.s.line.a.y = shape.s.line.b.y = y;
    shape.s.line.colour = colour;
    return vidAddToQueue(queue, &shape) != NULL;
}

bool vidLine(queue_t *queue, point_t from, point_t to, colour_t colour)
{
    vid_shape_t shape;
    shape.shape = VID_SHAPE_LINE;
    shape.s.line.a = from;
    shape.s.line.b = to;
    shape.s.line.colour = colour;
    return vidAddToQueue(queue, &shape) != NULL;
}

bool vidPutPixel(queue_t *queue, int x, int y, colour_t colour)
{
    vid_shape_t shape;
    shape.shape = VID_SHAPE_PUTPIXEL;
    shape.s.pix.point.x = x;
    shape.s.pix.point.y = y;
    shape.s.pix.colour = colour;
    return vidAddToQueue(queue, &shape) != NULL;
}

/*! \brief  Draws a filled rectangle */
void glFillRect(MGLreal left, MGLreal top, MGLreal right, MGLreal bottom)
{
    point_t topLeft, bottomRight;

    CCV;

    if (right < left)
	swap_MGLreal(&left, &right);
    if (bottom < top)
	swap_MGLreal(&top, &bottom);

    if (!mglMapToSurface(left, top, &topLeft) ||
	!mglMapToSurface(right, bottom, &bottomRight))
	return;

    vidFillRect(&current->render_queue, topLeft, bottomRight, current->colour);
}

/*! \brief  Clears the current rendering context */
void glClear(void)
{
    point_t topLeft, bottomRight;
    
    CCV;

    topLeft.x = topLeft.y = 0;
    bottomRight.x = current->surf_width;
    bottomRight.y = current->surf_height;
    vidFillRect(&current->render_queue, topLeft, bottomRight, current->clear_colour);
}

/*! \brief  Sets the current position */
void glMoveTo(MGLreal x, MGLreal y)
{
    CCV;
    current->pos.x = x;
    current->pos.y = y;
}

/*! \brief  Draws a line from the current position */
void glLineTo(MGLreal x, MGLreal y)
{
    point_t to, from;

    CCV;

    mglMapToSurface(x, y, &to);
    mglMapToSurface(current->pos.x, current->pos.y, &from);

    if (from.x == to.x)
	vidVLine(&current->render_queue, from.x, from.y, to.y, current->colour);
    else if (from.y == to.y)
	vidHLine(&current->render_queue, from.x, to.x, from.y, current->colour);
    else
	vidLine(&current->render_queue, from, to, current->colour);

    current->pos.x = x;
    current->pos.y = y;
}

/*! \brief  Draws an outlined rectangle */
void glRectangle(MGLreal left, MGLreal top, MGLreal right, MGLreal bottom)
{
    point_t topLeft, bottomRight;
    
    CCV;

    if (right < left)
	swap_MGLreal(&left, &right);
    if (bottom < top)
	swap_MGLreal(&top, &bottom);

    if (!mglMapToSurface(left, top, &topLeft) ||
	!mglMapToSurface(right, bottom, &bottomRight))
	return;

    /* Top */
    vidHLine(&current->render_queue, 
	topLeft.x, bottomRight.x, topLeft.y, current->colour);
    /* Bottom */
    vidHLine(&current->render_queue, 
	topLeft.x, bottomRight.x, bottomRight.y, current->colour);
    /* Left */
    vidVLine(&current->render_queue, 
	topLeft.x, topLeft.y + 1, bottomRight.y, current->colour);
    /* Right */
    vidVLine(&current->render_queue, 
	bottomRight.x, topLeft.y, bottomRight.y + 1, current->colour);
}

/*! \brief  Sets a single pixel */
void glPutPixel(MGLreal x, MGLreal y)
{
    point_t pt;

    CCV;

    mglMapToSurface(x, y, &pt);
    vidPutPixel(&current->render_queue, pt.x, pt.y, current->colour);
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
    for (; n > 0; s++, n--)
    	if (*s == c)
	    return (wchar_t*) s;
    return NULL;
}

void glDrawText(const MGLrect *rc, const wchar_t *str, int len)
{
    params_vid_t params;
    point_t topLeft, bottomRight;
    fileop_t op;
    rect_t temp;
    
    CCV;

    glFlush();
    
    if (len == (size_t) -1)
	len = wcslen(str);

    mglMapToSurface(rc->left, rc->top, &topLeft);
    mglMapToSurface(rc->right, rc->bottom, &bottomRight);

    if (current->vid_clip.num_rects == 0)
    {
        temp.left = temp.top = 0;
        temp.right = current->surf_width;
        temp.bottom = current->surf_height;
        params.vid_textout.clip.num_rects = 1;
        params.vid_textout.clip.rects = &temp;
    }
    else
        params.vid_textout.clip = current->vid_clip;

    params.vid_textout.rect.left = topLeft.x;
    params.vid_textout.rect.top = topLeft.y;
    params.vid_textout.rect.right = bottomRight.x;
    params.vid_textout.rect.bottom = bottomRight.y;
    params.vid_textout.buffer = str;
    params.vid_textout.length = len * sizeof(wchar_t);
    params.vid_textout.foreColour = current->colour;
    params.vid_textout.backColour = -1;
    FsRequestSync(current->video, VID_TEXTOUT, &params, sizeof(params), &op);
}

void glGetTextSize(const wchar_t *str, int len, MGLpoint *size)
{
    params_vid_t params;
    fileop_t op;
    
    CCV;

    if (len == (size_t) -1)
	len = wcslen(str);

    params.vid_textout.rect.left = 0;
    params.vid_textout.rect.top = 0;
    params.vid_textout.rect.right = current->surf_width;
    params.vid_textout.rect.bottom = current->surf_height;
    params.vid_textout.buffer = str;
    params.vid_textout.length = len * sizeof(wchar_t);
    params.vid_textout.foreColour = params.vid_textout.backColour = -1;
    FsRequestSync(current->video, VID_TEXTOUT, &params, sizeof(params), &op);

    mglMapToVirtual(params.vid_textout.rect.right, params.vid_textout.rect.bottom, size);
}

void glFillPolygon(const MGLpoint *points, unsigned num_points)
{
    point_t *pts;
    unsigned i;
    params_vid_t params;
    fileop_t op;
    rect_t temp;

    CCV;

    pts = malloc(sizeof(point_t) * num_points);
    if (pts == NULL)
	return;

    for (i = 0; i < num_points; i++)
    	mglMapToSurface(points[i].x, points[i].y, pts + i);
    
    glFlush();

    if (current->vid_clip.num_rects == 0)
    {
        temp.left = temp.top = 0;
        temp.right = current->surf_width;
        temp.bottom = current->surf_height;
        params.vid_fillpolygon.clip.num_rects = 1;
        params.vid_fillpolygon.clip.rects = &temp;
    }
    else
        params.vid_fillpolygon.clip = current->vid_clip;

    params.vid_fillpolygon.points = pts;
    params.vid_fillpolygon.length = num_points * sizeof(point_t);
    params.vid_fillpolygon.colour = current->colour;
    FsRequestSync(current->video, VID_FILLPOLYGON, &params, sizeof(params), &op);

    free(pts);
}

void glPolygon(const MGLpoint *points, unsigned num_points)
{
    MGLpoint firstPoint;
    unsigned i;
    
    for (i = 0; i < num_points; i++)
    {
    	if (i == 0)
	{
	    glMoveTo(points[i].x, points[i].y);
	    firstPoint = points[i];
	}
	else
	    glLineTo(points[i].x, points[i].y);
    }

    if (num_points > 0)
	glLineTo(firstPoint.x, firstPoint.y);
}

MGLcolour clrLighter(MGLcolour clr, unsigned char diff)
{
    int r, g, b;
    r = MGL_RED(clr) + diff;
    g = MGL_GREEN(clr) + diff;
    b = MGL_BLUE(clr) + diff;
    return MGL_COLOUR(min(r, 255), min(g, 255), min(b, 255));
}

MGLcolour clrDarker(MGLcolour clr, unsigned char diff)
{
    int r, g, b;
    r = MGL_RED(clr) - diff;
    g = MGL_GREEN(clr) - diff;
    b = MGL_BLUE(clr) - diff;
    return MGL_COLOUR(max(r, 0), max(g, 0), max(b, 0));
}

void glBevel(const MGLrect *rect, MGLcolour colour, int border, unsigned char diff, bool is_extruded)
{
    MGLpoint pts[6];

    glSetColour(is_extruded ? clrLighter(colour, diff) : clrDarker(colour, diff));
    pts[0].x = rect->right; pts[0].y = rect->top;
    pts[1].x = rect->left; pts[1].y = rect->top;
    pts[2].x = rect->left; pts[2].y = rect->bottom;
    pts[3].x = rect->left + border; pts[3].y = rect->bottom - border;
    pts[4].x = rect->left + border; pts[4].y = rect->top + border;
    pts[5].x = rect->right - border; pts[5].y = rect->top + border;
    glFillPolygon(pts, _countof(pts));

    glSetColour(is_extruded ? clrDarker(colour, diff) : clrLighter(colour, diff));
    pts[0].x = rect->right; pts[0].y = rect->top;
    pts[1].x = rect->right; pts[1].y = rect->bottom;
    pts[2].x = rect->left; pts[2].y = rect->bottom;
    pts[3].x = rect->left + border; pts[3].y = rect->bottom - border;
    pts[4].x = rect->right - border; pts[4].y = rect->bottom - border;
    pts[5].x = rect->right - border; pts[5].y = rect->top + border;
    glFillPolygon(pts, _countof(pts));
}
