/* $Id: vidtest.c,v 1.11 2002/12/18 22:58:18 pavlovskii Exp $ */

#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <stdio.h>
#include <math.h>

#include <os/syscall.h>
#include <os/defs.h>
#include <os/video.h>
#include <os/rtl.h>
#include <os/keyboard.h>

handle_t vid;
videomode_t mode;

void WaitForKeyPress(void)
{
    while (ConReadKey() & KBD_BUCKY_RELEASE)
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

int main(int argc, char **argv)
{
    params_vid_t params;
    void *mem;
    handle_t vidmem;

    vid = FsOpen(SYS_DEVICES L"/Classes/video0", FILE_READ | FILE_WRITE);
    if (vid == NULL)
    {
        _pwerror(SYS_DEVICES L"/Classes/video0");
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
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), NULL))
    {
        _pwerror(L"VID_SETMODE");
        HndClose(vid);
        return EXIT_FAILURE;
    }

    mode = params.vid_setmode;
    vidmem = VmmOpenSharedArea(mode.framebuffer);
    mem = VmmMapSharedArea(vidmem, NULL, VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
    if (mem != NULL)
        DoTestPatterns(mem);

    /*memset(&params, 0, sizeof(params));
    params.vid_setmode.width = 80;
    params.vid_setmode.height = 25;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), NULL))
        _pwerror(L"VID_SETMODE");*/

    HndClose(vidmem);
    HndClose(vid);
    return EXIT_SUCCESS;
}
