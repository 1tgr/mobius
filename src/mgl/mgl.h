#ifndef __INTERNAL_MGL_H
#define __INTERNAL_MGL_H

#include <gl/mgl.h>
#include <os/video.h>
#include <freetype/freetype.h>

typedef struct MGLpoint MGLpoint;
struct MGLpoint
{
	MGLreal x, y;
};

struct mglrc_t
{
	addr_t video;
	unsigned surf_width, surf_height;
	MGLreal gl_width, gl_height;
	MGLreal scale_x, scale_y;
	MGLcolour colour, clear_colour;
	MGLpoint pos;
	FT_Library ft_library;
	FT_Face ft_face;
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