/* $Id: vidtest.c,v 1.2 2002/03/05 02:10:21 pavlovskii Exp $ */

#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <stdio.h>

#include <os/syscall.h>
#include <os/defs.h>
#include <os/video.h>
#include <os/rtl.h>

handle_t vid;

bool VidFillRect(const rect_t *rect, colour_t clr)
{
    fileop_t op;
    params_vid_t params;
    vid_shape_t shape;

    shapes.shape = VID_SHAPE_FILLRECT;
    shapes.s.rect.rect = *rect;
    shapes.s.rect.colour = clr;
    params.vid_draw.shapes = &shape;
    params.vid_draw.length = sizeof(shape);
    if (!FsRequestSync(vid, VID_DRAW, &params, sizeof(params), &op))
    {
    	errno = op.result;
	return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    params_vid_t params;
    fileop_t op;
    vid_shape_t shapes[1];
    wchar_t str[] = L"Hello from vidtest!";
    rect_t rc;
    int dx, dy;
    videomode_t mode;
    
    vid = FsOpen(SYS_DEVICES L"/video", FILE_READ | FILE_WRITE);
    if (vid == NULL)
    {
	_pwerror(L"video");
	return EXIT_FAILURE;
    }

    memset(&mode, 0, sizeof(mode));
    mode.bitsPerPixel = 4;
    params.vid_setmode = mode;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), &op))
    {
	errno = op.result;
	_pwerror(L"VID_SETMODE");
    }

    mode = params.vid_setmode;

    shapes[0].shape = VID_SHAPE_LINE;
    shapes[0].s.line.a.x = 100;
    shapes[0].s.line.a.y = 200;
    shapes[0].s.line.b.x = 400;
    shapes[0].s.line.b.y = 50;
    shapes[0].s.line.colour = 9;

    params.vid_draw.shapes = shapes;
    params.vid_draw.length = sizeof(shapes);
    if (!FsRequestSync(vid, VID_DRAW, &params, sizeof(params), &op))
    {
	errno = op.result;
	_pwerror(L"VID_DRAW");
    }

    params.vid_textout.buffer = (addr_t) str;
    params.vid_textout.length = wcslen(str) * sizeof(wchar_t);
    params.vid_textout.x = 100;
    params.vid_textout.y = 100;
    params.vid_textout.foreColour = 0;
    params.vid_textout.backColour = 1;
    if (!FsRequestSync(vid, VID_TEXTOUT, &params, sizeof(params), &op))
    {
	errno = op.result;
	_pwerror(L"VID_TEXTOUT");
    }

    rc.left = rc.top = 0;
    rc.right = rc.bottom = 100;
    dx = dy = 1;
    for (;;)
    {
	VidFillRect(&rect, 0);

	if (rc.left <= 0 ||
	    rc.right > mode.width)
	    dx = -dx;

	if (rc.top <= 0 ||
	    rc.bottom > mode.height)
	    dy = -dy;

	rc.left += dx;
	rc.top += dy;
	rc.right += dx;
	rc.bottom += dy;

	VidFillRect(&rect, 15);
    }

    memset(&params, 0, sizeof(params));
    params.vid_setmode.width = 80;
    params.vid_setmode.height = 25;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), &op))
    {
	errno = op.result;
	_pwerror(L"VID_SETMODE");
    }

    FsClose(vid);
    return EXIT_SUCCESS;
}
