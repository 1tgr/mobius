/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/vmm.h>

#include "video.h"
#include "vgamodes.h"

uint8_t *vga_base;
uint8_t *const vga_base_global = PHYSICAL(0xA0000);

bool Vga4Init(void);
bool Vga4EnterMode(void *cookie, videomode_t *mode, const uint8_t *regs, 
				   const surface_vtbl_t **surf_vtbl, void **surf_cookie);
bool Vga8Init(void);
bool Vga8EnterMode(void *cookie, videomode_t *mode, const uint8_t *regs, 
				   const surface_vtbl_t **surf_vtbl, void **surf_cookie);

#define ACCEL_4	( VIDEO_ACCEL_FILLRECT | \
				  VIDEO_ACCEL_LINE | \
				  VIDEO_ACCEL_BLT_SCREEN_TO_SCREEN | \
				  VIDEO_ACCEL_BLT_SCREEN_TO_MEMORY | \
				  VIDEO_ACCEL_BLT_MEMORY_TO_SCREEN )

#define ACCEL_8	BPP8_ACCEL

static struct
{
	videomode_t mode;
	const uint8_t *regs;
} modes[] =
{
	/* cookie,  width,  height, bpp,	bpl,	flags,								regs */
	{ { 0x03,	 80,	 25,	4,		0,		VIDEO_MODE_TEXT,		0,			L"", },		mode03h },
	{ { 0x0d,	320,	200,	4,		0,		VIDEO_MODE_GRAPHICS,	ACCEL_4,	L"", },		mode0Dh },
	{ { 0x0e,	640,	200,	4,		0,		VIDEO_MODE_GRAPHICS,	ACCEL_4,	L"", },		mode0Eh },
	{ { 0x10,	640,	350,	4,		0,		VIDEO_MODE_GRAPHICS,	ACCEL_4,	L"", },		mode10h },
	{ { 0x12,	640,	480,	4,		0,		VIDEO_MODE_GRAPHICS,	ACCEL_4,	L"", },		mode12h },
	{ { 0x13,	320,	200,	8,		0,		VIDEO_MODE_GRAPHICS,	ACCEL_8,	L"fb_vga", },	mode13h },
};


static int VgaEnumModes(void *cookie, unsigned index, videomode_t *mode)
{
	if (index < _countof(modes))
	{
		*mode = modes[index].mode;
		return index == _countof(modes) - 1 ? VID_ENUM_STOP : VID_ENUM_CONTINUE;
	}
	else
		return VID_ENUM_ERROR;
}


static bool VgaEnterMode(void *cookie, videomode_t *mode, 
						 const surface_vtbl_t **surf_vtbl, void **surf_cookie)
{
	unsigned i;
	const uint8_t *regs;

	regs = NULL;
	for (i = 0; i < _countof(modes); i++)
        if (modes[i].mode.cookie == mode->cookie)
        {
            regs = modes[i].regs;
            break;
        }

    if (regs == NULL)
        return false;

	switch (mode->bitsPerPixel)
	{
	case 4:
		return Vga4EnterMode(cookie, mode, regs, surf_vtbl, surf_cookie);

	case 8:
		return Vga8EnterMode(cookie, mode, regs, surf_vtbl, surf_cookie);
	}

	return false;
}


static void VgaLeaveMode(void *cookie)
{
}


static const video_vtbl_t vga_vtbl =
{
	VgaEnumModes,
	VgaEnterMode,
	VgaLeaveMode,
	VidStoreVgaPalette,
	NULL,
};


static bool VgaAddDevice(dev_config_t *cfg, const video_vtbl_t **vtbl, void **cookie)
{
	*vtbl = &vga_vtbl;
	*cookie = NULL;
	return true;
}


bool DllMainCRTStartup(VIDEO_ADD_DEVICE *add_device)
{
	unsigned i;

	for (i = 0; i < _countof(modes); i++)
	{
		switch (modes[i].mode.bitsPerPixel)
		{
		case 4:
			modes[i].mode.bytesPerLine = modes[i].mode.width / 8;
			break;

		case 8:
			modes[i].mode.bytesPerLine = modes[i].mode.width;
			break;
		}
	}

	vga_base = VmmMap(0x20000 / PAGE_SIZE, NULL, (void*) 0xa0000,
		NULL, VM_AREA_MAP, VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
	VmmShare(vga_base, L"fb_vga");

	if (!Vga4Init())
		return false;
	if (!Vga8Init())
		return false;

	*add_device = VgaAddDevice;
	return true;
}
