/* $Id: rc.c,v 1.3 2002/03/06 19:31:41 pavlovskii Exp $ */

#include <stdlib.h>
#include <os/device.h>
#include <os/syscall.h>
#include <errno.h>
#include <stdio.h>
#include "mgl.h"

mglrc_t *current;

static bool vidStorePalette(handle_t video, const rgb_t *rgb, unsigned first, 
		     unsigned count)
{
    params_vid_t params;
    fileop_t op;
    params.vid_storepalette.entries = rgb;
    params.vid_storepalette.length = count * sizeof(*rgb);
    params.vid_storepalette.first_index = first;
    return FsRequestSync(video, VID_STOREPALETTE, &params, sizeof(params), &op);
}

static rgb_t palette[] =
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
};

/*! \brief  Creates a rendering context */
mglrc_t *mglCreateRc(const wchar_t *server)
{
    mglrc_t *rc;
    handle_t file;
    const wchar_t *font = L"helb____.ttf";
    int error;
    params_vid_t params;
    fileop_t op;

    if (server == NULL)
	server = SYS_DEVICES L"/video";

    wprintf(L"mglCreateRc: %s\n", server);

    rc = malloc(sizeof(*rc));
    if (rc == NULL)
	return NULL;

    memset(rc, 0, sizeof(*rc));

    rc->video = FsOpen(server, 0);
    if (rc->video == NULL /*||
	FT_Init_FreeType(&rc->ft_library) ||
	FT_New_Face(rc->ft_library,
	    "rezn000.ttf",
	    0,
	    &rc->ft_face)*/)
    {
	mglDeleteRc(rc);
	return NULL;
    }
     
    rc->ft_face = NULL;
    /*wprintf(L"Loading %s...", font);
    file = fsOpen(font);
    if (file)
    {
	void *buffer;
	size_t length;

	length = fsGetLength(file);
	buffer = malloc(length);

	wprintf(L"%u bytes\n", length);

	if (buffer != NULL)
	{
	    fsRead(file, buffer, &length);
	    error = FT_New_Memory_Face(rc->ft_library, buffer, length, 0, 
		&rc->ft_face);
	    if (error == FT_Err_Unknown_File_Format)
		wprintf(L"Unknown font file format\n");
	    else if (error)
		wprintf(L"Error reading font file (error code: %u)\n", error);
	    
	    free(buffer);
	}

	fsClose(file);
    }
    else
	_pwerror(font);
    
    if (rc->ft_face == NULL)
    {
	mglDeleteRc(rc);
	return NULL;
    }*/

    memset(&params, 0, sizeof(params));
    params.vid_setmode.bitsPerPixel = 8;
    if (!FsRequestSync(rc->video, VID_SETMODE, &params, sizeof(params), &op) ||
	op.result != 0)
    {
	errno = op.result;
	mglDeleteRc(rc);
	return NULL;
    }

    QueueInit(&rc->render_queue);
    rc->surf_width = params.vid_setmode.width;
    rc->surf_height = params.vid_setmode.height;
    rc->gl_width = 1280;
    rc->gl_height = 960;/*(rc->gl_width * rc->surf_height) / rc->surf_width;*/
    rc->scale_x = rc->gl_width / rc->surf_width;
    rc->scale_y = rc->gl_height / rc->surf_height;
    rc->colour = 0xffffff;
    rc->clear_colour = 0;
    /*vidStorePalette(rc->video, palette, 0, 8);
    vidStorePalette(rc->video, palette + 8, 56, 8);*/

    if (current == NULL)
	mglUseRc(rc);

    return rc;
}

/*! \brief  Deletes a rendering context */
bool mglDeleteRc(mglrc_t *rc)
{
    wprintf(L"mglDeleteRc: %p\n", rc);
    if (rc)
    {
	vidFlushQueue(&rc->render_queue, rc->video);

	if (rc->video)
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
    wprintf(L"mglUseRc: %p\n", rc);

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

bool mgliMapToSurface(MGLreal x, MGLreal y, point_t *pt)
{
    if (current)
    {
	pt->x = (int) (x / current->scale_x);
	pt->y = (int) (y / current->scale_y);
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

    vidFlushQueue(&current->render_queue, current->video);
}

/*! @} */