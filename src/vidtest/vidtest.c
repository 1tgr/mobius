/* $Id: vidtest.c,v 1.4 2002/03/05 16:22:26 pavlovskii Exp $ */

#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <stdio.h>

#include <os/syscall.h>
#include <os/defs.h>
#include <os/video.h>
#include <os/rtl.h>

handle_t vid;
volatile bool key_pressed;

bool VidFillRect(const rect_t *rect, colour_t clr)
{
    fileop_t op;
    params_vid_t params;
    vid_shape_t shape;

    shape.shape = VID_SHAPE_FILLRECT;
    shape.s.rect.rect = *rect;
    shape.s.rect.colour = clr;
    params.vid_draw.shapes = &shape;
    params.vid_draw.length = sizeof(shape);
    if (!FsRequestSync(vid, VID_DRAW, &params, sizeof(params), &op))
    {
    	errno = op.result;
	return false;
    }

    return true;
}

void KeyboardThread(void *param)
{
    params_vid_t params;
    wchar_t ch;
    uint32_t key;
    fileop_t op;

    params.vid_textout.x = 0;
    params.vid_textout.y = 0;
    
    do
    {
	ch = key = ConReadKey();
	
	if (ch != 0)
	{
	    params.vid_textout.buffer = (addr_t) &ch;
	    params.vid_textout.length = sizeof(ch);
	    params.vid_textout.foreColour = 15;
	    params.vid_textout.backColour = (colour_t) -1;
	    if (!FsRequestSync(vid, VID_TEXTOUT, &params, sizeof(params), &op))
	    {
		errno = op.result;
		_pwerror(L"VID_TEXTOUT");
	    }

	    params.vid_textout.x += 8;
	}
    } while (key != 27);

    key_pressed = true;
    ThrExitThread(0);
}

int main(int argc, char **argv)
{
    params_vid_t params;
    fileop_t op;
    vid_shape_t shapes[2];
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

    if (argc >= 2)
	mode.width = atoi(argv[1]);
    
    if (argc >= 3)
	mode.height = atoi(argv[2]);
    
    if (argc >= 4)
	mode.bitsPerPixel = atoi(argv[3]);
    
    params.vid_setmode = mode;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), &op))
    {
	errno = op.result;
	_pwerror(L"VID_SETMODE");
	FsClose(vid);
	return EXIT_FAILURE;
    }

    mode = params.vid_setmode;

    shapes[0].shape = VID_SHAPE_LINE;
    shapes[0].s.line.a.x = 0;
    shapes[0].s.line.a.y = 0;
    shapes[0].s.line.b.x = mode.width;
    shapes[0].s.line.b.y = mode.height;
    shapes[0].s.line.colour = 9;

    shapes[1].shape = VID_SHAPE_LINE;
    shapes[1].s.line.a.x = mode.width;
    shapes[1].s.line.a.y = 0;
    shapes[1].s.line.b.x = 0;
    shapes[1].s.line.b.y = mode.height;
    shapes[1].s.line.colour = 9;

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

    rc.left = rc.top = 150;
    rc.right = rc.bottom = 400;
    if (rc.right >= mode.width)
	rc.right = mode.width;
    if (rc.bottom >= mode.height)
	rc.bottom = mode.height;
    VidFillRect(&rc, 14);

    rc.left = rc.top = 0;
    rc.right = rc.bottom = 100;
    dx = dy = 1;
    ThrCreateThread(KeyboardThread, NULL, 10);
    while (!key_pressed)
    {
	VidFillRect(&rc, 0);

	if (rc.left <= 0 && dx < 0)
	    dx = -dx;
	else if (rc.right > mode.width && dx > 0)
	    dx = -dx;

	if (rc.top <= 0 && dy < 0)
	    dy = -dy;
	else if (rc.bottom > mode.height && dy > 0)
	    dy = -dy;

	rc.left += dx;
	rc.top += dy;
	rc.right += dx;
	rc.bottom += dy;

	VidFillRect(&rc, 15);
	ThrSleep(50);
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
