#include "mgl.h"
#include "render.h"
#include <errno.h>
#include <stdio.h>

void swap_MGLreal(MGLreal *a, MGLreal *b)
{
	MGLreal temp = *b;
	*b = *a;
	*a = temp;
}

bool vidFillRect(addr_t video, point_t topLeft, point_t bottomRight, pixel_t colour)
{
	vid_request_t req;
	status_t hr;
	rect_t rc;

	rc.left = topLeft.x;
	rc.top = topLeft.y;
	rc.right = bottomRight.x;
	rc.bottom = bottomRight.y;

	req.header.code = VID_FILLRECT;
	req.params.vid_fillrect.rect = rc;
	req.params.vid_fillrect.colour = colour;
	
	hr = devUserRequestSync(video, (request_t*) &req, sizeof(req));
	if (hr)
	{
		errno = hr;
		return false;
	}
	else
		return true;
}

bool vidVLine(addr_t video, int x, int y1, int y2, pixel_t colour)
{
	vid_request_t req;
	status_t hr;

	req.header.code = VID_VLINE;
	req.params.vid_vline.a.x = 
		req.params.vid_vline.b.x = x;
	req.params.vid_vline.a.y = y1;
	req.params.vid_vline.b.y = y2;
	req.params.vid_vline.colour = colour;
	
	hr = devUserRequestSync(video, (request_t*) &req, sizeof(req));
	if (hr)
	{
		errno = hr;
		return false;
	}
	else
		return true;
}

bool vidHLine(addr_t video, int x1, int x2, int y, pixel_t colour)
{
	vid_request_t req;
	status_t hr;
	
	req.header.code = VID_HLINE;
	req.params.vid_hline.a.x = x1;
	req.params.vid_hline.b.x = x2;
	req.params.vid_hline.a.y =
		req.params.vid_hline.b.y = y;
	req.params.vid_hline.colour = colour;
	
	hr = devUserRequestSync(video, (request_t*) &req, sizeof(req));
	if (hr)
	{
		errno = hr;
		return false;
	}
	else
		return true;
}

bool vidLine(addr_t video, point_t from, point_t to, pixel_t colour)
{
	vid_request_t req;
	status_t hr;
	
	req.header.code = VID_LINE;
	req.params.vid_line.a = from;
	req.params.vid_line.b = to;
	req.params.vid_line.colour = colour;
	
	hr = devUserRequestSync(video, (request_t*) &req, sizeof(req));
	if (hr)
	{
		errno = hr;
		return false;
	}
	else
		return true;
}

bool vidPutPixel(addr_t video, int x, int y, pixel_t colour)
{
	vid_request_t req;
	status_t hr;
	
	req.header.code = VID_PUTPIXEL;
	req.params.vid_putpixel.point.x = x;
	req.params.vid_putpixel.point.y = y;
	req.params.vid_putpixel.colour = colour;
	
	hr = devUserRequestSync(video, (request_t*) &req, sizeof(req));
	if (hr)
	{
		errno = hr;
		return false;
	}
	else
		return true;
}

/*
static pixel_t dm[] = 
{
	0,	12,	3,	15,
	8,	4,	11,	7,
	2,	14,	1,	13,
	10,	6,	9,	5
};

static pixel_t dither(int x, int y, pixel_t level)
{
     return level > dm[(x % 4) + 4 * (y % 4)];
}
*/

pixel_t vidColourMatch(addr_t video, int x, int y, MGLcolour clr)
{
	//return dither(x, y, clr) ? 15 : 0;
	return clr % 16;
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

	vidFillRect(current->video, topLeft, bottomRight, 
		vidColourMatch(current->video, 0, 0, current->colour));
}

void glClear()
{
	point_t topLeft, bottomRight;
	FT_UInt glyph_index;
	int x, y;
	
	CCV;

	topLeft.x = topLeft.y = 0;
	bottomRight.x = current->surf_width;
	bottomRight.y = current->surf_height;
	vidFillRect(current->video, topLeft, bottomRight, 
		vidColourMatch(current->video, 0, 0, current->clear_colour));

	glyph_index = FT_Get_Char_Index(current->ft_face, 'A');
	wprintf(L"glyph_index = %d\n", glyph_index);
	if (FT_Load_Glyph(current->ft_face, glyph_index, FT_LOAD_DEFAULT) == 0)
	{
		FT_GlyphSlot slot = current->ft_face->glyph; 
		FT_Render_Glyph(current->ft_face->glyph, 0);
		for (x = 0; x < slot->bitmap.rows; x++)
			for (y = 0; y < slot->bitmap.width; y++)
				vidPutPixel(current->video, x, y, 
					slot->bitmap.buffer[x + y * slot->bitmap.width]);
	}
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
		vidVLine(current->video, from.x, from.y, to.y, 
			vidColourMatch(current->video, 0, 0, current->colour));
	else if (from.y == to.y)
		vidHLine(current->video, from.x, to.x, from.y, 
			vidColourMatch(current->video, 0, 0, current->colour));
	else
		vidLine(current->video, from, to, 
			vidColourMatch(current->video, 0, 0, current->colour));

	current->pos.x = x;
	current->pos.y = y;
}

void glRectangle(MGLreal left, MGLreal top, MGLreal right, MGLreal bottom)
{
	point_t topLeft, bottomRight;
	pixel_t c;

	CCV;

	if (right < left)
		swap_MGLreal(&left, &right);
	if (bottom < top)
		swap_MGLreal(&top, &bottom);

	if (!mgliMapToSurface(left, top, &topLeft) ||
		!mgliMapToSurface(right, bottom, &bottomRight))
		return;

	c = vidColourMatch(current->video, 0, 0, current->colour);
	vidHLine(current->video, topLeft.x, bottomRight.x, topLeft.y, c);
	vidHLine(current->video, topLeft.x, bottomRight.x, bottomRight.y, c);
	vidVLine(current->video, topLeft.x, topLeft.y, bottomRight.y, c);
	vidVLine(current->video, bottomRight.x, topLeft.y, bottomRight.y, c);
}