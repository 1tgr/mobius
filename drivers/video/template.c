/* $Id: template.c,v 1.1 2002/12/21 09:49:07 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>

#include <wchar.h>

#include "video.h"

static void s3Close(video_t *vid)
{
}

static struct
{
    videomode_t mode;
} s3_modes[] =
{
    /*  cookie, width,  height, bpp, bpl,   flags,                  */
    { { 0,      640,    480,    8,   0,     VIDEO_MODE_GRAPHICS, } },
};

static int s3EnumModes(video_t *vid, unsigned index, videomode_t *mode)
{
    if (index < _countof(s3_modes))
    {
        s3_modes[index].mode.bytesPerLine = 
            (s3_modes[index].mode.width * s3_modes[index].mode.bitsPerPixel) / 8;
        *mode = s3_modes[index].mode;
        return index == _countof(s3_modes) - 1 ? VID_ENUM_STOP : VID_ENUM_CONTINUE;
    }
    else
        return VID_ENUM_ERROR;
}

static bool s3SetMode(video_t *vid, videomode_t *mode)
{
    bool found;
    unsigned i;
    
    found = false;
    for (i = 0; i < _countof(s3_modes); i++)
        if (s3_modes[i].mode.cookie == mode->cookie)
        {
            found = true;
            break;
        }
        
    if (!found)
        return false;

    return true;
}

static void s3PutPixel(video_t *vid, int x, int y, colour_t clr)
{
}

static colour_t s3GetPixel(video_t *vid, int x, int y)
{
    return 0;
}

static void s3HLine(video_t *vid, int x1, int x2, int y, colour_t clr)
{
}

static video_t s3 =
{
    s3Close,
    s3EnumModes,
    s3SetMode,
    s3PutPixel,
    s3GetPixel,
    s3HLine,
    NULL,           /* vline */
    NULL,           /* line */
    NULL,           /* fillrect */
    NULL,           /* textout */
    NULL,           /* fillpolygon */
    vgaStorePalette
};

video_t *s3Init(device_config_t *cfg)
{
    return &s3;
}
