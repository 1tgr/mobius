#include "mgl.h"
#include "render.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <os/syscall.h>

#undef __FTERRORS_H__
#define FT_ERRORDEF( e, v, s )	 { e, s },
#define FT_ERROR_START_LIST	 {
#define FT_ERROR_END_LIST	 { 0, 0 } };

const struct
{
    int 	 err_code;
    const char*  err_msg;
} ft_errors[] =

#include FT_ERRORS_H

#define vidAddToQueue(q, s)    QueueAppend(q, s, sizeof(*s))

bool vidFlushQueue(queue_t *queue, addr_t video)
{
    if (queue->length > 0)
    {
	params_vid_t params;
	fileop_t op;

	params.vid_draw.shapes = QUEUE_DATA(*queue);
	params.vid_draw.length = queue->length;
	if (FsRequestSync(video, VID_DRAW, &params, sizeof(params), &op))
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

const char *mgliGetFtError(int error)
{
    unsigned i;
    static char str[20];

    for (i = 0; i < _countof(ft_errors); i++)
	if (ft_errors[i].err_code == error)
	    return ft_errors[i].err_msg;

    sprintf(str, "unknown 0x%d", error);
    return str;
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

void glFillRect(MGLreal left, MGLreal top, MGLreal right, MGLreal bottom)
{
    point_t topLeft, bottomRight;

    CCV;

    if (right < left)
	swap_MGLreal(&left, &right);
    if (bottom < top)
	swap_MGLreal(&top, &bottom);

    if (!mgliMapToSurface(left, top, &topLeft) ||
	!mgliMapToSurface(right, bottom, &bottomRight))
	return;

    vidFillRect(&current->render_queue, topLeft, bottomRight, current->colour);
}

void glClear(void)
{
    point_t topLeft, bottomRight;
    FT_UInt glyph_index;
    int x, y;
    FT_Error error;
    
    CCV;

    topLeft.x = topLeft.y = 0;
    bottomRight.x = current->surf_width;
    bottomRight.y = current->surf_height;
    vidFillRect(&current->render_queue, topLeft, bottomRight, current->clear_colour);

    /*FT_Set_Pixel_Sizes(current->ft_face, 0, 20);

    glyph_index = FT_Get_Char_Index(current->ft_face, 'a');
    wprintf(L"glyph_index = %d\n", glyph_index);
    error = FT_Load_Glyph(current->ft_face, glyph_index, FT_LOAD_DEFAULT);
    if (error)
	wprintf(L"FT_Load_Glyph: %S\n", mgliGetFtError(error));
    else
    {
	FT_GlyphSlot slot = current->ft_face->glyph; 
	error = FT_Render_Glyph(current->ft_face->glyph, 0);
	if (error)
	    wprintf(L"FT_Render_Glyph: %S\n", mgliGetFtError(error));
	else
	{
	    for (x = 0; x < slot->bitmap.rows; x++)
		for (y = 0; y < slot->bitmap.width; y++)
		    vidPutPixel(current->video, x, y, 
			slot->bitmap.buffer[x + y * slot->bitmap.width]);
	}
    }*/
}

void glMoveTo(MGLreal x, MGLreal y)
{
    CCV;
    current->pos.x = x;
    current->pos.y = y;
}

void glLineTo(MGLreal x, MGLreal y)
{
    point_t to, from;

    CCV;

    mgliMapToSurface(x, y, &to);
    mgliMapToSurface(current->pos.x, current->pos.y, &from);

    if (from.x == to.x)
	vidVLine(&current->render_queue, from.x, from.y, to.y, current->colour);
    else if (from.y == to.y)
	vidHLine(&current->render_queue, from.x, to.x, from.y, current->colour);
    else
	vidLine(&current->render_queue, from, to, current->colour);

    current->pos.x = x;
    current->pos.y = y;
}

void glRectangle(MGLreal left, MGLreal top, MGLreal right, MGLreal bottom)
{
    point_t topLeft, bottomRight;
    
    CCV;

    if (right < left)
	swap_MGLreal(&left, &right);
    if (bottom < top)
	swap_MGLreal(&top, &bottom);

    if (!mgliMapToSurface(left, top, &topLeft) ||
	!mgliMapToSurface(right, bottom, &bottomRight))
	return;

    vidHLine(&current->render_queue, 
	topLeft.x, bottomRight.x, topLeft.y, current->colour);
    vidHLine(&current->render_queue, 
	topLeft.x, bottomRight.x, bottomRight.y, current->colour);
    vidVLine(&current->render_queue, 
	topLeft.x, topLeft.y, bottomRight.y, current->colour);
    vidVLine(&current->render_queue, 
	bottomRight.x, topLeft.y, bottomRight.y, current->colour);
}

void glPutPixel(MGLreal x, MGLreal y)
{
    point_t pt;

    CCV;

    mgliMapToSurface(x, y, &pt);
    vidPutPixel(&current->render_queue, pt.x, pt.y, current->colour);
}