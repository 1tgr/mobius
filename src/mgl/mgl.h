/* $Id: mgl.h,v 1.5 2002/05/05 13:51:30 pavlovskii Exp $ */

#ifndef __INTERNAL_MGL_H
#define __INTERNAL_MGL_H

#include <gl/mgl.h>
#include <os/video.h>
#include <os/queue.h>
#include "render.h"

struct mglrc_t
{
    handle_t video;
    unsigned surf_width, surf_height;
    MGLreal gl_width, gl_height;
    MGLreal scale_x, scale_y;
    MGLcolour colour, clear_colour;
    MGLpoint pos;
    queue_t render_queue;
    clip_t vid_clip;
    bool did_set_mode;
    videomode_t mode;
};

bool	    mglMapToSurface(MGLreal x, MGLreal y, point_t *pt);
bool	    mglMapToVirtual(int x, int y, MGLpoint *pt);

extern mglrc_t *current;

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
