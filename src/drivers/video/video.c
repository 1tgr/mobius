/* $Id: video.c,v 1.19 2002/12/18 23:19:04 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>

#include <errno.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>

#include <os/syscall.h>
#include <kernel/fs.h>

#include "video.h"
#include "vidfuncs.h"

void swap_int(int *a, int *b)
{
    int temp = *b;
    *b = *a;
    *a = temp;
}

videomode_t video_mode = { 0, 80, 25, 4, 0, VIDEO_MODE_TEXT };

typedef struct request_vid_t request_vid_t;
struct request_vid_t
{
    request_t header;
    params_vid_t params;
};

typedef struct video_drv_t video_drv_t;
struct video_drv_t
{
    device_t dev;
    video_t *vid;
};

video_t *vga4Init(device_config_t *cfg);
video_t *vga8Init(device_config_t *cfg);
video_t *VgapInit(device_config_t *cfg);
video_t *s3Init8(device_config_t *cfg);
video_t *s3Init16(device_config_t *cfg);

struct
{
    const wchar_t *name;
    video_t *(*init)(device_config_t *);
    video_t *vid;
} drivers[] =
{
    //{ L"vga4",     vga4Init, NULL },
    { L"vga8",      vga8Init, NULL },
    //{ L"vgaplane",  VgapInit, NULL },
    { L"s3_8",      s3Init8,  NULL },
    { L"s3_16",     s3Init16, NULL },
    { NULL,         NULL,     NULL },
};

typedef struct modemap_t modemap_t;
struct modemap_t
{
    videomode_t mode;
    video_t *vid;
};

modemap_t *modes;
unsigned numModes;

void vidMoveCursor(video_t *vid, point_t pt)
{
    rect_t rect;
    clip_t clip;
    rect.left = rect.top = 0;
    rect.right = video_mode.width;
    rect.bottom = video_mode.height;
    clip.rects = &rect;
    clip.num_rects = 1;
    vid->vidPutPixel(vid, &clip, pt.x, pt.y, 0xffffff);
}

int vidMatchMode(const videomode_t *a, const videomode_t *b)
{
    if ((b->width == 0 || a->width == b->width) &&
        (b->height == 0 || a->height == b->height) &&
        (b->bitsPerPixel == 0 || a->bitsPerPixel == b->bitsPerPixel))
        return a->width + a->height + a->bitsPerPixel;
    else
        return 0;
}

bool vidSetMode(video_drv_t *video, videomode_t *mode)
{
    int i, best, score, s;
    video_t *vid;

    if (video->vid)
    {
        video->vid->vidClose(video->vid);
        video->vid = NULL;
    }

    best = -1;
    score = 0;
    for (i = 0; i < numModes; i++)
    {
        s = vidMatchMode(&modes[i].mode, mode);
        wprintf(L"video: mode %u = %ux%ux%u: score = %d\n", 
            i, modes[i].mode.width, modes[i].mode.height, modes[i].mode.bitsPerPixel,
            s);
        if (s > score)
        {
            best = i;
            score = s;
        }
    }

    if (best == -1)
    {
        wprintf(L"video: mode %ux%ux%u not found\n", 
            mode->width, mode->height, mode->bitsPerPixel);
        errno = ENOTFOUND;
        return false;
    }

    *mode = modes[best].mode;
    vid = modes[best].vid;

    assert(vid->vidClose != NULL);
    assert(vid->vidEnumModes != NULL);
    assert(vid->vidSetMode != NULL);

    if (vid->vidMoveCursor == NULL)
        vid->vidMoveCursor = vidMoveCursor;
    
    if (!vid->vidSetMode(vid, mode))
        return false;

    video->vid = vid;
    wprintf(L"video: using mode %ux%ux%u\n", 
        mode->width, mode->height, mode->bitsPerPixel);

    return true;
}

bool vidRequest(device_t* dev, request_t* req)
{
    video_drv_t *video = (video_drv_t*) dev;
    params_vid_t *params = &((request_vid_t*) req)->params;
    
    switch (req->code)
    {
    case DEV_REMOVE:
        free(dev);
            return true;

    case VID_SETMODE:
        if (!vidSetMode(video, &params->vid_setmode))
        {
            req->result = errno;
            return false;
        }
        else
            return true;
        
    case VID_GETMODE:
        params->vid_getmode = video_mode;
        return true;

    case VID_STOREPALETTE:
    {
        vid_palette_t *p = &params->vid_storepalette;
        if (video->vid->vidStorePalette)
        {
            video->vid->vidStorePalette(video->vid, 
                p->entries,
                p->first_index,
                p->length / sizeof(rgb_t));
            return true;
        }
        break;
    }

    case VID_MOVECURSOR:
        if (video->vid->vidMoveCursor)
        {
            video->vid->vidMoveCursor(video->vid, params->vid_movecursor);
            return true;
        }
        break;
    }

    req->result = ENOTIMPL;
    return false;
}

static const device_vtbl_t vid_vtbl =
{
    vidRequest,
    NULL,
    NULL,
};

/*
 * Support routines for FreeType
 */

FILE *fopen(const char *file, const char *mode)
{
    wchar_t *wc;
    size_t len;
    handle_t hnd;

    assert(mode[0] == 'r' && mode[1] == 'b' && mode[2] == '\0');

    len = strlen(file);
    wc = malloc(sizeof(wchar_t) * (len + 1));
    if (wc == NULL)
        return NULL;

    len = mbstowcs(wc, file, len);
    if (len == -1)
    {
        free(wc);
        return NULL;
    }

    wc[len] = '\0';
    wprintf(L"fopen: opening %s\n", wc);
    hnd = FsOpen(wc, FILE_READ);
    free(wc);

    return (FILE*) hnd;
}

int fclose(FILE *file)
{
    if (file != NULL)
    {
        HndClose(NULL, (handle_t) file, 'file');
        return 0;
    }
    else
        return EOF;
}

int fseek(FILE *stream, long offset, int origin)
{
    return FsSeek((handle_t) stream, offset, origin);
}

long ftell(FILE *stream)
{
    return FsSeek((handle_t) stream, 0, SEEK_CUR);
}

size_t fread(void *buffer, size_t size, size_t count, FILE *stream)
{
    size_t bytes_read;
    if (FsReadSync((handle_t) stream, buffer, size * count, &bytes_read))
        return bytes_read / size;
    else
        return 0;
}

void vidAddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
    video_drv_t* dev;
    int i, j, code;
    videomode_t mode;
    video_t *vid;
    
    for (i = 0; drivers[i].name; i++)
    {
        vid = drivers[i].vid = drivers[i].init(cfg);
        if (vid == NULL)
            continue;

        j = 0;
        do
        {
            code = vid->vidEnumModes(vid, j, &mode);
            if (code == VID_ENUM_ERROR)
                break;

            numModes++;
            modes = realloc(modes, sizeof(modemap_t) * numModes);
            modes[numModes - 1].vid = vid;
            modes[numModes - 1].mode = mode;
            j++;
        } while (code != VID_ENUM_STOP);
    }

    dev = malloc(sizeof(video_drv_t));
    DevInitDevice(&dev->dev, &vid_vtbl, drv, 0);
    dev->vid = NULL;

    if (cfg->device_class == 0)
        cfg->device_class = 0x0300;

    DevAddDevice(&dev->dev, name, cfg);
}

bool DrvInit(driver_t* drv)
{
    drv->add_device = vidAddDevice;
    /*vidInitText();*/
    return true;
}
