/* $Id: rc.c,v 1.8 2002/04/20 12:47:28 pavlovskii Exp $ */

#include <stdlib.h>
#include <os/device.h>
#include <os/syscall.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include "mgl.h"

/*!
 *  \ingroup    mgl
 */

mglrc_t *current;

/*static bool vidStorePalette(handle_t video, const rgb_t *rgb, unsigned first, unsigned count)
{
    params_vid_t params;
    fileop_t op;
    params.vid_storepalette.entries = rgb;
    params.vid_storepalette.length = count * sizeof(*rgb);
    params.vid_storepalette.first_index = first;
    return FsRequestSync(video, VID_STOREPALETTE, &params, sizeof(params), &op);
}

const rgb_t palette[16 + 50] =
{
    { 0, 0, 0, 0 },
    { 128, 0, 0, 0 },
    { 0, 128, 0, 0 },
    { 128, 128, 0, 0 },
    { 0, 0, 128, 0 },
    { 128, 0, 128, 0 },
    { 0, 128, 128, 0 },
    { 128, 128, 128, 0 },
    { 192, 192, 192, 0 },
    { 255, 0, 0, 0 },
    { 0, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 0, 0, 255, 0 },
    { 255, 0, 255, 0 },
    { 0, 255, 255, 0 },
    { 255, 255, 255, 0 },

    // banner colours
    { 61, 35, 190, 0 },
    { 56, 34, 194, 0 },
    { 58, 34, 192, 0 },
    { 78, 40, 175, 0 },
    { 69, 38, 182, 0 },
    { 64, 36, 187, 0 },
    { 67, 37, 185, 0 },
    { 75, 39, 177, 0 },
    { 72, 38, 180, 0 },
    { 83, 42, 170, 0 },
    { 80, 41, 173, 0 },
    { 86, 42, 168, 0 },
    { 89, 43, 165, 0 },
    { 91, 44, 163, 0 },
    { 94, 45, 160, 0 },
    { 97, 46, 158, 0 },
    { 102, 47, 153, 0 },
    { 100, 47, 156, 0 },
    { 105, 48, 151, 0 },
    { 111, 50, 146, 0 },
    { 108, 49, 148, 0 },
    { 113, 51, 143, 0 },
    { 116, 51, 141, 0 },
    { 119, 52, 139, 0 },
    { 127, 55, 131, 0 },
    { 124, 54, 134, 0 },
    { 122, 53, 136, 0 },
    { 133, 56, 126, 0 },
    { 135, 57, 124, 0 },
    { 141, 59, 119, 0 },
    { 138, 58, 122, 0 },
    { 155, 63, 107, 0 },
    { 152, 62, 109, 0 },
    { 146, 60, 114, 0 },
    { 144, 60, 117, 0 },
    { 149, 61, 112, 0 },
    { 157, 64, 105, 0 },
    { 168, 67, 95, 0 },
    { 171, 68, 92, 0 },
    { 174, 69, 90, 0 },
    { 191, 74, 75, 0 },
    { 188, 73, 78, 0 },
    { 179, 70, 85, 0 },
    { 182, 71, 83, 0 },
    { 177, 69, 88, 0 },
    { 185, 72, 80, 0 },
    { 160, 65, 102, 0 },
    { 163, 65, 100, 0 },
    { 166, 66, 97, 0 },
    { 130, 56, 129, 0 },
};*/

/*! \brief  Creates a rendering context */
mglrc_t *mglCreateRc(const wchar_t *server)
{
    mglrc_t *rc;
    params_vid_t params;
    fileop_t op;

    if (server == NULL)
	server = SYS_DEVICES L"/video";

    printf("mglCreateRc: %S\n", server);

    rc = malloc(sizeof(*rc));
    if (rc == NULL)
	return NULL;

    memset(rc, 0, sizeof(*rc));

    rc->video = FsOpen(server, 0);
    if (rc->video == NULL)
    {
        printf("mglCreateRc: FsOpen failed\n");
	mglDeleteRc(rc);
	return NULL;
    }

    if (!FsRequestSync(rc->video, VID_GETMODE, &params, sizeof(params), &op))
    {
        printf("mglCreateRc: VID_GETMODE failes\n");
        errno = op.result;
        mglDeleteRc(rc);
        return NULL;
    }

    if (params.vid_getmode.flags & VIDEO_MODE_TEXT)
    {
        printf("mglCreateRc: Current mode is text, switching to graphics mode\n");
        memset(&params, 0, sizeof(params));
        //params.vid_setmode.bitsPerPixel = 8;
        rc->did_set_mode = true;
        //params.vid_setmode.height = 200;
        if (!FsRequestSync(rc->video, VID_SETMODE, &params, sizeof(params), &op) ||
	    op.result != 0)
        {
	    errno = op.result;
	    mglDeleteRc(rc);
	    return NULL;
        }
    }
    else
        printf("mglCreateRc: Already in graphics mode\n");

    QueueInit(&rc->render_queue);
    rc->surf_width = params.vid_setmode.width;
    rc->surf_height = params.vid_setmode.height;
    rc->gl_width = 1280;
    rc->gl_height = 960;/*(rc->gl_width * rc->surf_height) / rc->surf_width;*/
    rc->scale_x = rc->gl_width / rc->surf_width;
    rc->scale_y = rc->gl_height / rc->surf_height;
    rc->colour = 0xffffff;
    rc->clear_colour = 0;

    /*if (params.vid_setmode.bitsPerPixel == 4)
    {
	vidStorePalette(rc->video, palette, 0, 8);
	vidStorePalette(rc->video, palette + 8, 56, 8);
    }
    else if (params.vid_setmode.bitsPerPixel == 8)
    	vidStorePalette(rc->video, palette, 0, _countof(palette));*/
    
    if (current == NULL)
	mglUseRc(rc);

    return rc;
}

/*! \brief  Deletes a rendering context */
bool mglDeleteRc(mglrc_t *rc)
{
    printf("mglDeleteRc: %p\n", rc);
    if (rc)
    {
	vidFlushQueue(&rc->render_queue, rc);
        free(rc->vid_clip.rects);

	if (rc->video && rc->did_set_mode)
	{
	    params_vid_t params;
	    fileop_t op;
	    
	    memset(&params, 0, sizeof(params));
	    params.vid_setmode.width = 80;
	    params.vid_setmode.height = 25;
	    FsRequestSync(rc->video, VID_SETMODE, &params, sizeof(params), &op);
	    FsClose(rc->video);
	}

	free(rc);
    
	if (rc == current)
	    current = NULL;

	return true;
    }
    else
	return false;
}

/*! \brief  Makes another rendering context current */
bool mglUseRc(mglrc_t *rc)
{
    printf("mglUseRc: %p\n", rc);

    if (rc == NULL)
    {
	errno = EINVALID;
	return false;
    }

    glFlush();
    current = rc;
    return true;
}

/*! \brief  Retrieves the dimensions of the specified rendering context */
bool mglGetDimensions(mglrc_t *rc, MGLrect *rect)
{
    if (rc == NULL)
    {
	CCB;
	rc = current;
    }

    rect->left = rect->top = 0;
    rect->right = rc->gl_width;
    rect->bottom = rc->gl_height;
    return true;
}

bool mglMapToSurface(MGLreal x, MGLreal y, point_t *pt)
{
    if (current)
    {
	pt->x = (int) (x * current->surf_width) / current->gl_width;
	pt->y = (int) (y * current->surf_height) / current->gl_height;
	return true;
    }
    else
    {
	errno = EINVALID;
	return false;
    }
}

bool mglMapToVirtual(int x, int y, MGLpoint *pt)
{
    if (current)
    {
	pt->x = (int) (x * current->gl_width) / current->surf_width;
	pt->y = (int) (y * current->gl_height) / current->surf_height;
	return true;
    }
    else
    {
	errno = EINVALID;
	return false;
    }
}

/*! \brief  Changes the colour of the current rendering context */
MGLcolour glSetColour(MGLcolour clr)
{
    MGLcolour old;

    if (current == NULL)
    {
	errno = EINVALID;
	return false;
    }

    old = current->colour;
    current->colour = clr;
    return old;
}

/*! \brief  Changes the clear colour of the current rendering context */
MGLcolour glSetClearColour(MGLcolour clr)
{
    MGLcolour old;

    if (current == NULL)
    {
	errno = EINVALID;
	return false;
    }

    old = current->colour;
    current->clear_colour = clr;
    return old;
}

/*!
 *  \brief  Ensures that any drawing commands queued on the current rendering 
 *  context have been executed
 */
void glFlush(void)
{
    CCV;

    vidFlushQueue(&current->render_queue, current);
}

void mglMoveCursor(MGLpoint pt)
{
    params_vid_t params;
    fileop_t op;

    CCV;

    memset(&params, 0, sizeof(params));
    mglMapToSurface(pt.x, pt.y, &params.vid_movecursor);
    FsRequestSync(current->video, VID_MOVECURSOR, &params, sizeof(params), &op);
}

void mglSetClip(mglrc_t *rc, const MGLclip *clip)
{
    unsigned i;
    point_t pt;

    if (rc == NULL)
    {
        CCV;
        rc = current;
    }

    free(rc->vid_clip.rects);
    rc->vid_clip.rects = malloc(sizeof(rect_t) * clip->num_rects);
    rc->vid_clip.num_rects = clip->num_rects;

    for (i = 0; i < rc->vid_clip.num_rects; i++)
    {
        mglMapToSurface(clip->rects[i].left, clip->rects[i].top, &pt);
        rc->vid_clip.rects[i].left = max(pt.x, 0);
        rc->vid_clip.rects[i].top = max(pt.y, 0);

        mglMapToSurface(clip->rects[i].right, clip->rects[i].bottom, &pt);
        rc->vid_clip.rects[i].right = min(pt.x, rc->surf_width);
        rc->vid_clip.rects[i].bottom = min(pt.y, rc->surf_height);
    }
}
