/*
 * $History: vidtest.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:29
 * Updated in $/coreos/apps/vidtest
 * Stopped using ConReadKey
 * Stopped using file handle in async I/O
 * Added history block
 */

#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <os/syscall.h>
#include <os/defs.h>
#include <os/video.h>
#include <os/rtl.h>
#include <os/keyboard.h>

handle_t vid;
videomode_t mode;


void WaitForKeyPress(void)
{
    uint32_t key;

    while (FsRead(ProcGetProcessInfo()->std_in,
        &key,
        0,
        sizeof(key),
        NULL) && 
        (key & KBD_BUCKY_RELEASE) != 0)
        ;
}


static inline uint16_t s3ColourToPixel16(colour_t clr)
{
    uint8_t r, g, b;
    r = COLOUR_RED(clr);
    g = COLOUR_GREEN(clr);
    b = COLOUR_BLUE(clr);
    /* 5-6-5 R-G-B */
    return ((r >> 3) << 11) |
           ((g >> 2) << 5) |
           ((b >> 3) << 0);
}


void DoTestPatterns(void *mem)
{
    unsigned x, y, one_third, n;
    point_t centre;

    union
    {
        uint8_t *p8;
        uint16_t *p16;
        void *v;
    } pix;

    one_third = mode.height / 3;
    centre.x = mode.width / 2;
    centre.y = mode.height / 2;

    pix.v = mem;
    for (y = 0; y < mode.height; y++)
    {
        for (x = 0; x < mode.width; x++)
        {
            switch (mode.bitsPerPixel)
            {
            case 8:
                pix.p8[x] = x * y;
                break;
            case 16:
                pix.p16[x] = s3ColourToPixel16(MAKE_COLOUR(x * x, y * y, x * y));
                break;
            }
        }

        pix.v = (uint8_t*) pix.v + mode.bytesPerLine;
    }

    WaitForKeyPress();

    pix.v = mem;
    one_third = mode.height / 3;
    for (y = 0; y < mode.height; y++)
    {
        for (x = 0; x < mode.width; x++)
        {
            switch (mode.bitsPerPixel)
            {
            case 8:
                pix.p8[x] = x;
                break;
            case 16:
                if (y < one_third)
                    pix.p16[x] = s3ColourToPixel16(MAKE_COLOUR(x, 0, 0));
                else if (y < one_third * 2)
                    pix.p16[x] = s3ColourToPixel16(MAKE_COLOUR(0, x, 0));
                else
                    pix.p16[x] = s3ColourToPixel16(MAKE_COLOUR(0, 0, x));
                break;
            }
        }

        pix.v = (uint8_t*) pix.v + mode.bytesPerLine;
    }

    WaitForKeyPress();

    pix.v = mem;
    for (y = 0; y < mode.height; y++)
    {
        for (x = 0; x < mode.width; x++)
        {
            n = (unsigned) sqrt((x - centre.x) * (x - centre.x) + 
                (y - centre.y) * (y - centre.y));
            switch (mode.bitsPerPixel)
            {
            case 8:
                pix.p8[x] = n;
                break;
            case 16:
                pix.p16[x] = s3ColourToPixel16(MAKE_COLOUR(n, n * 2, n * 4));
                break;
            }
        }

        pix.v = (uint8_t*) pix.v + mode.bytesPerLine;
    }

    WaitForKeyPress();

    memset(mem, 0, mode.height * mode.bytesPerLine);
}


void DoAccelTestPatterns(void)
{
    vid_shape_t shapes[20];
    params_vid_t params;
    uint32_t key;
    fileop_t op;
    unsigned i;

    op.event = EvtCreate(false);
    FsReadAsync(ProcGetProcessInfo()->std_in, &key, 0, sizeof(key), &op);
    while (true)
    {
        if (EvtIsSignalled(op.event))
        {
            if ((key & KBD_BUCKY_RELEASE) == 0)
                break;
            else
                FsReadAsync(ProcGetProcessInfo()->std_in, &key, 0, sizeof(key), &op);
        }

        for (i = 0; i < _countof(shapes); i++)
        {
            shapes[i].shape = VID_SHAPE_FILLRECT;
            shapes[i].s.rect.colour = MAKE_COLOUR(rand() % 255, rand() % 255, rand() % 255);
            shapes[i].s.rect.rect.left = rand() % mode.width;
            shapes[i].s.rect.rect.top = rand() % mode.height;
            shapes[i].s.rect.rect.right = 
                shapes[i].s.rect.rect.left + rand() % (mode.width - shapes[i].s.rect.rect.left);
            shapes[i].s.rect.rect.bottom = 
                shapes[i].s.rect.rect.top + rand() % (mode.height - shapes[i].s.rect.rect.top);
            if (shapes[i].s.rect.rect.right > mode.width)
                shapes[i].s.rect.rect.right = mode.width;
            if (shapes[i].s.rect.rect.bottom > mode.height)
                shapes[i].s.rect.rect.bottom = mode.height;
        }

        params.vid_draw.shapes = shapes;
        params.vid_draw.length = sizeof(shapes);
        if (!FsRequestSync(vid, VID_DRAW, &params, sizeof(params), NULL))
            _wdprintf(L"vidtest: VID_DRAW failed: %s\n", _wcserror(errno));
    }

    HndClose(op.event);
}


int main(int argc, char **argv)
{
    params_vid_t params;
    void *mem;
    handle_t vidmem;
    videomode_t old_mode;

    vid = FsOpen(SYS_DEVICES L"/Classes/video0", FILE_READ | FILE_WRITE);
    if (vid == NULL)
    {
        _pwerror(SYS_DEVICES L"/Classes/video0");
        return EXIT_FAILURE;
    }

    if (!FsRequestSync(vid, VID_GETMODE, &params, sizeof(params), NULL))
    {
        _pwerror(L"VID_GETMODE");
        HndClose(vid);
        return EXIT_FAILURE;
    }

    old_mode = params.vid_getmode;

    memset(&mode, 0, sizeof(mode));

    if (argc >= 2)
        mode.width = atoi(argv[1]);

    if (argc >= 3)
        mode.height = atoi(argv[2]);

    if (argc >= 4)
        mode.bitsPerPixel = atoi(argv[3]);

    params.vid_setmode = mode;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), NULL))
    {
        _pwerror(L"VID_SETMODE (set)");
        HndClose(vid);
        return EXIT_FAILURE;
    }

    mode = params.vid_setmode;

    if (mode.bitsPerPixel <= 4)
        DoAccelTestPatterns();
    else
    {
        vidmem = HndOpen(mode.framebuffer);
        mem = VmmMapSharedArea(vidmem, NULL, VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
        if (mem != NULL)
            DoTestPatterns(mem);

        HndClose(vidmem);
    }

    memset(&params, 0, sizeof(params));
    params.vid_setmode = old_mode;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), NULL))
        _pwerror(L"VID_SETMODE (restore)");

    HndClose(vid);
    return EXIT_SUCCESS;
}
