/* $Id: vidtest.c,v 1.8 2002/08/17 22:52:14 pavlovskii Exp $ */

#include <stdlib.h>
#include <errno.h>
#include <wchar.h>
#include <stdio.h>

#include <os/syscall.h>
#include <os/defs.h>
#include <os/video.h>
#include <os/rtl.h>

handle_t vid;
videomode_t mode;

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

int main(int argc, char **argv)
{
    params_vid_t params;
    fileop_t op;
    void *mem;
    unsigned x, y;
    union
    {
        uint8_t *p8;
        uint16_t *p16;
        void *v;
    } pix;
    
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
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), &op))
    {
        errno = op.result;
        _pwerror(L"VID_SETMODE");
        FsClose(vid);
        return EXIT_FAILURE;
    }

    mode = params.vid_setmode;
    mem = VmmMapShared(mode.framebuffer, NULL, MEM_READ | MEM_WRITE);
    if (mem != NULL)
    {
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
