#ifndef __INTERNAL_MGL_H
#define __INTERNAL_MGL_H

#include <gl/mgl.h>
#include <os/video.h>
#include <os/queue.h>
#include <freetype/freetype.h>
#include "render.h"

struct mglrc_t
{
	handle_t video;
	unsigned surf_width, surf_height;
	MGLreal gl_width, gl_height;
	MGLreal scale_x, scale_y;
	MGLcolour colour, clear_colour;
	MGLpoint pos;
	FT_Library ft_library;
	FT_Face ft_face;
	queue_t render_queue;
};

extern mglrc_t *current;

bool	mgliMapToSurface(MGLreal x, MGLreal y, point_t *pt);

#define CCV \
	if (current == NULL) \
		return;

#define CCB \
	if (current == NULL) \
	{ \
		errno = EINVALID; \
		return false; \
	}

#endif