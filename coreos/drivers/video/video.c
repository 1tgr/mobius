/* $Id: video.c,v 1.3 2003/06/22 15:43:38 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/proc.h>

#include <errno.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>

#include <kernel/fs.h>
#include <kernel/profile.h>

#include "video.h"
#include "vidfuncs.h"
#include "vga.h"

void swap_int(int *a, int *b)
{
    int temp = *b;
    *b = *a;
    *a = temp;
}

typedef struct request_vid_t request_vid_t;
struct request_vid_t
{
    request_t header;
    params_vid_t params;
};


typedef struct video_driver_t video_driver_t;
struct video_driver_t
{
	void *cookie;
	const video_vtbl_t *vtbl;
	module_t *module;
	unsigned num_modes;
	videomode_t *modes;
};


typedef struct video_t video_t;
struct video_t
{
    device_t *device;
	videomode_t mode;
	unsigned num_drivers;
    video_driver_t *drivers;
	video_driver_t *owner;
	surface_t *surf;
	surface_t front_surf;
};


static bool video_suppress_mode_switch;
//static mutex_t *mux_vga;
static spinlock_t spin_vga;


bool Bpp8Init(void);

void VidLockVga(void)
{
	//MuxAcquire(mux_vga);
	SpinAcquire(&spin_vga);
}


void VidUnlockVga(void)
{
	//MuxRelease(mux_vga);
	SpinRelease(&spin_vga);
}


void VidWriteVgaRegisters(const uint8_t *regs)
{
    unsigned i;
    volatile uint8_t a;

    if (video_suppress_mode_switch)
        return;

    VidLockVga();
    
    /* Send MISC regs */
    out(VGA_MISC_WRITE, *regs++);
    out(VGA_INSTAT_READ, *regs++);
    
    /* Send SEQ regs*/
    for (i = 0; i < 5; i++)
    {
        out(VGA_SEQ_INDEX, i);
        out(VGA_SEQ_DATA, *regs++);
    }
    
    /* Clear Protection bits */
    out16(VGA_CRTC_INDEX, 0xe11);
    
    /* Send CRTC regs */
    for (i = 0; i < 25; i++)
    {
        out(VGA_CRTC_INDEX, i);
        out(VGA_CRTC_DATA, *regs++);
    }

    /* Send GRAPHICS regs */
    for (i = 0; i < 9; i++)
    {
        out(VGA_GC_INDEX, i);
        out(VGA_GC_DATA, *regs++);
    }
    
    a = in(VGA_INSTAT_READ);
    
    /* Send ATTRCON regs */
    for (i = 0; i < 21; i++)
    {
        a = in(VGA_AC_INDEX);
        out(VGA_AC_INDEX, i);
        out(VGA_AC_WRITE, *regs++);
    }
    
    out(VGA_AC_WRITE, 0x20);

	VidUnlockVga();
}


void VidStoreVgaPalette(void *cookie, const rgb_t *entries, unsigned first, unsigned count)
{
    unsigned Index;

    /* start with palette entry 0 */
    VidLockVga();
    out(VGA_PALETTE_MASK, 0xff);
    out(VGA_DAC_WRITE_INDEX, first);
    for (Index = 0; Index < count; Index++)
    {
        out(VGA_DAC_DATA, entries[Index].red >> 2); /* red */   
        out(VGA_DAC_DATA, entries[Index].green >> 2); /* green */
        out(VGA_DAC_DATA, entries[Index].blue >> 2); /* blue */
    }

    VidUnlockVga();
}


static int VidMatchMode(const videomode_t *a, const videomode_t *b)
{
    if ((b->width == 0 || a->width == b->width) &&
        (b->height == 0 || a->height == b->height) &&
        (b->bitsPerPixel == 0 || a->bitsPerPixel == b->bitsPerPixel))
        return a->width + a->height + a->bitsPerPixel;
    else
        return 0;
}


static status_t VidSetMode(video_t *video, videomode_t *mode)
{
    int i, j, best_driver, best_mode, score, s;
    video_driver_t *driver;
	surface_t *surf;

    if (video->owner != NULL)
    {
        video->owner->vtbl->VidLeaveMode(video->owner->cookie);
        video->owner = NULL;
    }

    best_driver = -1;
	best_mode = -1;
    score = 0;
    for (i = 0; i < video->num_drivers; i++)
		for (j = 0; j < video->drivers[i].num_modes; j++)
		{
			s = VidMatchMode(&video->drivers[i].modes[j], mode);
			wprintf(L"video: mode %u/%u = %ux%ux%u: score = %d\n", 
				i, j,
				video->drivers[i].modes[j].width, 
				video->drivers[i].modes[j].height, 
				video->drivers[i].modes[j].bitsPerPixel,
				s);
			if (s > score)
			{
				best_driver = i;
				best_mode = j;
				score = s;
			}
		}

    if (best_driver == -1)
    {
        wprintf(L"video: mode %ux%ux%u not found\n", 
            mode->width, mode->height, mode->bitsPerPixel);
        return ENOTFOUND;
    }

	driver = video->drivers + best_driver;
    *mode = driver->modes[best_mode];

	surf = &video->front_surf;
    if (!driver->vtbl->VidEnterMode(driver->cookie, 
		mode, 
		&surf->vtbl, 
		&surf->cookie))
        return EINVALID;

    video->owner = driver;
	video->surf = surf;
	video->mode = *mode;
    wprintf(L"video: using mode %ux%ux%u\n", 
        mode->width, mode->height, mode->bitsPerPixel);

    return 0;
}


static void VidDeleteDevice(device_t *device)
{
	video_t *video;
	unsigned i;

	video = device->cookie;

	for (i = 0; i < video->num_drivers; i++)
		free(video->drivers[i].modes);

	free(video->drivers);
	free(video);
}


#define VID_OP(code, type) \
    case code: \
    { \
        type *shape; \
        shape = (type*) &buf->s; \
        \
        
#define VID_END_OP \
        break; \
    }

static status_t VidRequest(device_t* device, request_t* req)
{
    video_t *video = device->cookie;
    params_vid_t *params = &((request_vid_t*) req)->params;

    switch (req->code)
    {
    case VID_SETMODE:
        return VidSetMode(video, &params->vid_setmode);

    case VID_GETMODE:
        params->vid_getmode = video->mode;
        return 0;

    case VID_STOREPALETTE:
        {
            vid_palette_t *p = &params->vid_storepalette;
            video->owner->vtbl->VidStorePalette(video->owner->cookie, 
                p->entries,
                p->first_index,
                p->length / sizeof(rgb_t));
            return 0;
        }

    case VID_DRAW:
		if (video->surf == NULL)
			return 0;
		else
        {
            vid_shape_t *buf;
            size_t user_length;

            user_length = params->vid_draw.length;
            params->vid_draw.length = 0;
            buf = params->vid_draw.shapes;

            while (params->vid_draw.length < user_length)
            {
                switch (buf->shape)
                {
                VID_OP(VID_SHAPE_FILLRECT, vid_rect_t)
					if (video->mode.accel & VIDEO_ACCEL_FILLRECT)
						video->surf->vtbl->SurfFillRect(video->surf, 
							shape->rect.left, shape->rect.top, 
							shape->rect.right, shape->rect.bottom, 
							shape->colour);
					else
						return ENOTIMPL;
                VID_END_OP

                VID_OP(VID_SHAPE_HLINE, vid_line_t)
                    if (shape->a.x != shape->b.x)
                    {
                        if (shape->b.x < shape->a.x)
                            swap_int(&shape->a.x, &shape->b.x);
						if (video->mode.accel & VIDEO_ACCEL_LINE)
							video->surf->vtbl->SurfHLine(video->surf, 
								shape->a.x, shape->b.x, 
								shape->a.y, 
								shape->colour);
						else
							return ENOTIMPL;
                    }
                VID_END_OP

                VID_OP(VID_SHAPE_VLINE, vid_line_t)
                    if (shape->a.y != shape->b.y)
                    {
                        if (shape->b.y < shape->a.y)
                            swap_int(&shape->a.y, &shape->b.y);
						if (video->mode.accel & VIDEO_ACCEL_LINE)
							video->surf->vtbl->SurfVLine(video->surf, 
								shape->a.x, 
								shape->a.y, shape->b.y, 
								shape->colour);
						else
							return ENOTIMPL;
                    }
                VID_END_OP

                VID_OP(VID_SHAPE_LINE, vid_line_t)
					if (video->mode.accel & VIDEO_ACCEL_LINE)
						video->surf->vtbl->SurfLine(video->surf,
							shape->a.x, shape->a.y, 
							shape->b.x, shape->b.y, 
							shape->colour);
					else
						return ENOTIMPL;
                VID_END_OP

                VID_OP(VID_SHAPE_PUTPIXEL, vid_pixel_t)
                    video->surf->vtbl->SurfPutPixel(video->surf, 
                        shape->point.x, shape->point.y,
                        shape->colour);
                VID_END_OP

                VID_OP(VID_SHAPE_GETPIXEL, vid_pixel_t)
                    shape->colour = video->surf->vtbl->SurfGetPixel(video->surf, 
                        shape->point.x, shape->point.y);
                VID_END_OP
                }

                buf++;
                params->vid_draw.length += sizeof(*buf);
            }

            return 0;
        }

    /*case VID_FILLPOLYGON:
		if (video->surf == NULL)
			return 0;
		else
		{
			video->surf->vtbl->SurfFillPolygon(video->surf->cookie,
				params->vid_fillpolygon.points, 
				params->vid_fillpolygon.length / sizeof(point_t),
				params->vid_fillpolygon.colour);
			return 0;
		}*/

    case VID_MOVECURSOR:
		if (video->owner == NULL)
			return 0;
		else if (video->mode.accel & VIDEO_ACCEL_CURSOR)
		{
            video->owner->vtbl->VidMoveCursor(video->owner->cookie, params->vid_movecursor);
            return 0;
        }
		else
			return ENOTIMPL;

	case VID_BLT_SCREEN_TO_SCREEN:
		if (video->owner == NULL)
			return 0;
		else if (video->mode.accel & VIDEO_ACCEL_BLT_SCREEN_TO_SCREEN)
		{
			video->surf->vtbl->SurfBltScreenToScreen(video->surf, 
				&params->vid_blt.dest.rect, 
				&params->vid_blt.src.rect);
			return 0;
		}
		else
			return ENOTIMPL;

	case VID_BLT_SCREEN_TO_MEMORY:
		if (video->surf == NULL)
			return 0;
		else if (video->mode.accel & VIDEO_ACCEL_BLT_SCREEN_TO_MEMORY)
		{
			video->surf->vtbl->SurfBltScreenToMemory(video->surf, 
				params->vid_blt.dest.memory.buffer, 
				params->vid_blt.dest.memory.pitch,
				&params->vid_blt.src.rect);
			return 0;
		}
		else
			return ENOTIMPL;

	case VID_BLT_MEMORY_TO_SCREEN:
		if (video->surf == NULL)
			return 0;
		else if (video->mode.accel & VIDEO_ACCEL_BLT_MEMORY_TO_SCREEN)
		{
			video->surf->vtbl->SurfBltMemoryToScreen(video->surf, 
				&params->vid_blt.dest.rect,
				params->vid_blt.src.memory.buffer, 
				params->vid_blt.src.memory.pitch);
			return 0;
		}
		else
			return ENOTIMPL;
    }

    return ENOTIMPL;
}

static const device_vtbl_t vid_vtbl =
{
	VidDeleteDevice,
    VidRequest,
    NULL,
    NULL,
};

void VidAddDevice(driver_t* drv, const wchar_t* name, dev_config_t *cfg)
{
	static const videomode_t default_mode =
	{
		0, 80, 25, 16, (80 * 16) / 8, VIDEO_MODE_TEXT, 0, L""
	};

	wchar_t value_name[10];
	const wchar_t *driver_name;
    video_t *video;
	bool (*entry)(VIDEO_ADD_DEVICE*);
	VIDEO_ADD_DEVICE add_device;
	video_driver_t *driver;
	unsigned i, j;
	int code;
	videomode_t mode;

    video_suppress_mode_switch = 
		ProGetBoolean(cfg->profile_key, L"SuppressModeSwitch", false);

	video = malloc(sizeof(*video));
	if (video == NULL)
		return;

	if (cfg->device_class == 0)
        cfg->device_class = 0x0300;

	video->mode = default_mode;
	video->num_drivers = 0;
	video->drivers = NULL;
	video->owner = NULL;
	video->surf = NULL;

    i = 0;
	while (true)
    {
		swprintf(value_name, L"%u", i);
		driver_name = ProGetString(cfg->profile_key, value_name, NULL);
		if (driver_name == NULL)
			break;

		i++;

		video->drivers = realloc(video->drivers, 
			sizeof(*video->drivers) * (video->num_drivers + 1));
		driver = video->drivers + video->num_drivers;
		driver->cookie = NULL;
		driver->vtbl = NULL;
		driver->num_modes = 0;
		driver->modes = NULL;

		driver->module = PeLoad(NULL, driver_name, 0);
		if (driver->module == NULL)
			continue;

		entry = (void*) driver->module->entry;
		add_device = NULL;
		if (!entry(&add_device) ||
			add_device == NULL ||
			!add_device(cfg, 
				&driver->vtbl, 
				&driver->cookie))
		{
			wprintf(L"video: %s failed to load\n", driver_name);
			PeUnload(NULL, driver->module);
			continue;
		}

		assert(driver->vtbl->VidEnumModes != NULL);
		assert(driver->vtbl->VidEnterMode != NULL);
		assert(driver->vtbl->VidStorePalette != NULL);
		assert(driver->vtbl->VidLeaveMode != NULL);

        j = 0;
        do
        {
            code = driver->vtbl->VidEnumModes(driver->cookie, j, &mode);
            if (code == VID_ENUM_ERROR)
                break;

            driver->modes = realloc(driver->modes, 
				sizeof(videomode_t) * (driver->num_modes + 1));
            driver->modes[driver->num_modes] = mode;
			driver->num_modes++;
            j++;
        } while (code != VID_ENUM_STOP);

		video->num_drivers++;
    }

	video->device = DevAddDevice(drv, &vid_vtbl, 0, name, cfg, video);
}


bool DrvInit(driver_t* drv)
{
	//mux_vga = MuxCreate();
	SpinInit(&spin_vga);

	if (!Bpp8Init())
		return false;

    drv->add_device = VidAddDevice;
    return true;
}
