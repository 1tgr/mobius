/* $Id: vidtest.c,v 1.1 2002/03/05 01:56:05 pavlovskii Exp $ */

#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <stdio.h>

#include <os/syscall.h>
#include <os/defs.h>
#include <os/video.h>
#include <os/rtl.h>

int main(int argc, char **argv)
{
    handle_t vid;
    params_vid_t params;
    fileop_t op;
    vid_shape_t shapes[1];
    wchar_t str[] = L"Hello from vidtest!";
    
    vid = FsOpen(SYS_DEVICES L"/video", FILE_READ | FILE_WRITE);
    if (vid == NULL)
    {
	_pwerror(L"video");
	return EXIT_FAILURE;
    }

    memset(&params, 0, sizeof(params));
    params.vid_setmode.bitsPerPixel = 4;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), &op))
    {
	errno = op.result;
	_pwerror(L"VID_SETMODE");
    }

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

    ConReadKey();

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
