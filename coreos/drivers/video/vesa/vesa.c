/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/syscall.h>
#include <kernel/vmm.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "video.h"
#include "biosvid.h"
#include "bpp8.h"


typedef struct vesa_v86data_t vesa_v86data_t;
struct vesa_v86data_t
{
	union
	{
		uint8_t jmp[3];
		uint32_t alignment;
	} u;
	uint32_t command_event;
	uint32_t status_event;
	uint16_t command;
	uint16_t param;
	uint16_t status;
	uint16_t info_offset;
	uint16_t mode_offset;

	/* VBE 2.0+ */
	uint16_t oem_version;
	FARPTR vendor_name;
	FARPTR product_name;
	FARPTR product_revision;
};

#pragma pack(push, 2)
typedef struct vesa_info_t vesa_info_t;
struct vesa_info_t
{
	uint8_t signature[4];
	uint16_t version;
	uint32_t oem_name;
	uint32_t capabilities;
	FARPTR modes;
	uint16_t memory_size;
};

typedef struct vesa_mode_t vesa_mode_t;
struct vesa_mode_t
{
	uint16_t attributes;
	uint8_t attributes_win_a;
	uint8_t attributes_win_b;
	uint16_t window_granularity;
	uint16_t window_size;
	uint16_t window_seg_a;
	uint16_t window_seg_b;
	FARPTR window_posn_function;
	uint16_t bytes_per_line;

	uint16_t width;
	uint16_t height;
	uint8_t char_width;
	uint8_t char_height;
	uint8_t num_planes;
	uint8_t bits_per_pixel;
	uint8_t num_banks;
	uint8_t memory_type;
	uint8_t bank_size;
	uint8_t num_pages;
	uint8_t reserved;

	/* VBE 1.2+ */
	uint8_t red_width;
	uint8_t red_shift;
	uint8_t green_width;
	uint8_t green_shift;
	uint8_t blue_width;
	uint8_t blue_shift;
	uint8_t reserved_width;
	uint8_t reserved_shift;
	uint8_t direct_screen_info;

	/* VBE 2.0+ */
	uint32_t linear_memory_phys;
	FARPTR offscreen_ptr;
	uint16_t offscreen_size;
};

#pragma pack(pop)

CASSERT(sizeof(vesa_mode_t) == 0x32);

thread_t *vesa_thread;
vesa_v86data_t *vesa_data;
addr_t vesa_code, vesa_stack;
unsigned vesa_num_modes;

struct
{
	videomode_t mode;
	uint8_t *base;
} *vesa_modes;


static event_t *command_event, *status_event;


#define COMMAND_EXIT			0
#define COMMAND_CHECK			1
#define COMMAND_GET_MODE_INFO	2
#define COMMAND_SET_MODE		3

static bool VesaInit(void)
{
	uint16_t *code, *stack, *stack_top;
	FARPTR fpcode, fpstack_top;

	code = (uint16_t*) vesa_code = MemAllocLow();
	stack = (uint16_t*) vesa_stack = MemAllocLow();
	stack_top = stack + 0x800;
	fpcode = MK_FP((uint16_t) ((addr_t) code >> 4), 0);
	fpstack_top = MK_FP((uint16_t) ((addr_t) stack >> 4), 0x1000);
	wprintf(L"VesaInit: code = %p (%04x:%04x), stack = %p (%04x:%04x)\n",
		code, FP_SEG(fpcode), FP_OFF(fpcode),
		stack_top, FP_SEG(fpstack_top), FP_OFF(fpstack_top));

	memcpy(code, biosvid, sizeof(biosvid));

	vesa_data = PHYSICAL(code);
	assert(vesa_data->u.jmp[0] == 0xe9);
	assert(vesa_data->command_event == 0xaabbccdd);
	assert(vesa_data->status_event == 0xeeff0011);

	vesa_data->command_event = WrapEvtCreate(false);
	vesa_data->status_event = WrapEvtCreate(false);
	vesa_data->command = 0;
	vesa_data->status = 0;

	command_event = (event_t*) HndRef(NULL, vesa_data->command_event, 'evnt');
	assert(command_event != NULL);
	status_event = (event_t*) HndRef(NULL, vesa_data->status_event, 'evnt');
	assert(status_event != NULL);

	vesa_thread = i386CreateV86Thread(fpcode,
		fpstack_top,
		16,
		NULL,
		L"VesaV86Thread");

	return vesa_thread != NULL;
}


static uint16_t VesaRequest(uint16_t command, uint16_t param)
{
	assert(vesa_thread != NULL);
	assert(vesa_data != NULL);

	vesa_data->command = command;
	vesa_data->param = param;
	EvtSignal(command_event, true);

	if (command == COMMAND_EXIT)
	{
		ThrWaitHandle(ThrGetCurrent(), &vesa_thread->hdr);
		vesa_thread = NULL;
	}
	else
		ThrWaitHandle(ThrGetCurrent(), &status_event->hdr);

	KeYield();

	return vesa_data->status;
}


static void VesaCleanup(void)
{
	if (vesa_thread != NULL)
	{
		VesaRequest(COMMAND_EXIT, 0);
		vesa_thread = NULL;

		MemFreeLow(vesa_code);
		MemFreeLow(vesa_stack);
		HndClose(&command_event->hdr);
		HndClose(&status_event->hdr);
	}
}


static int VesaEnumModes(void *cookie, unsigned index, videomode_t *mode)
{
	if (index < vesa_num_modes)
	{
		*mode = vesa_modes[index].mode;
		return index == vesa_num_modes - 1 ? VID_ENUM_STOP : VID_ENUM_CONTINUE;
	}
	else
		return VID_ENUM_ERROR;
}


static bool VesaEnterMode(void *cookie, 
						  videomode_t *mode, 
						  const surface_vtbl_t **surf_vtbl,
						  void **surf_cookie)
{
	uint16_t result;
	unsigned i;
	uint8_t *base;

	base = NULL;
	for (i = 0; i < vesa_num_modes; i++)
		if (mode->cookie == vesa_modes[i].mode.cookie)
		{
			base = vesa_modes[i].base;
			break;
		}

	if (base == NULL)
		return false;

	result = VesaRequest(COMMAND_SET_MODE, mode->cookie);
	if ((uint8_t) result != 0x4f ||
		(uint8_t) (result >> 8) != 0)
	{
		wprintf(L"VesaInit: function 4F02 failed: AH = %x, AL = %x\n", 
			(uint8_t) (result >> 8), (uint8_t) result);
		return false;
	}

	VidStoreVgaPalette(cookie, Bpp8GetPalette(), 0, 256);
	return Bpp8CreateSurface(mode, base, surf_vtbl, surf_cookie);
}


static void VesaLeaveMode(void *cookie)
{
	VesaRequest(COMMAND_SET_MODE, 0x12);
}


static bool VesaIsModeSuitable(const vesa_mode_t *mode)
{
	const wchar_t *reason;

	reason = NULL;
	if ((mode->attributes & 1) == 0)
		reason = L"Mode not supported";
	/*else if ((mode->attributes & 2) == 0)
		reason = L"No optional information present";*/
	else if ((mode->attributes & 128) == 0)
		reason = L"Linear framebuffer not supported";
	else if (mode->bits_per_pixel != 8)
		reason = L"Colour depth not supported by driver";
	else if (mode->width > 1024 || mode->height > 768)
		reason = L"Resolution too high";

	if (reason == NULL)
		return true;
	else
	{
		wprintf(L"VesaIsModeSuitable(%ux%ux%u): %s\n",
			mode->width, mode->height, mode->bits_per_pixel, reason);
		return false;
	}
}


static video_vtbl_t vesa_vtbl =
{
	VesaEnumModes,
	VesaEnterMode,
	VesaLeaveMode,
	VidStoreVgaPalette,
	NULL,					/* movecursor */
};


static bool VesaAddDevice(dev_config_t *cfg, const video_vtbl_t **vtbl, void **cookie)
{
	*vtbl = &vesa_vtbl;
	*cookie = NULL;
	return true;
}


bool DllMainCRTStartup(VIDEO_ADD_DEVICE *add_device)
{
	uint16_t result;
	vesa_info_t *info;
	vesa_mode_t *modes;
	uint16_t *numbers;
	unsigned i, j, k, num_modes;
	wchar_t fb_name[] = L"fb_vesa_00000000";
	uint8_t *video_base;

	if (vesa_thread == NULL && 
		!VesaInit())
		return NULL;

	result = VesaRequest(COMMAND_CHECK, 0);
	if ((uint8_t) result != 0x4f ||
		(uint8_t) (result >> 8) != 0)
	{
		wprintf(L"VesaInit: function 4F00 failed: AH = %x, AL = %x\n", 
			(uint8_t) (result >> 8), (uint8_t) result);
		goto error0;
	}

	info = (vesa_info_t*) ((char*) vesa_data + vesa_data->info_offset);
	wprintf(L"VesaInit: version %x, modes at %04x:%04x\n",
		info->version, FP_SEG(info->modes), FP_OFF(info->modes));

	numbers = PHYSICAL(FP_TO_LINEAR(FP_SEG(info->modes), FP_OFF(info->modes)));
	for (i = 0; numbers[i] != 0xffff; i++)
		;

	num_modes = i;
	modes = malloc(sizeof(vesa_mode_t) * num_modes);
	if (modes == NULL)
		goto error1;

	vesa_num_modes = 0;
	for (i = 0; i < num_modes; i++)
	{
		result = VesaRequest(COMMAND_GET_MODE_INFO, numbers[i]);
		if ((uint8_t) result != 0x4f ||
			(uint8_t) (result >> 8) != 0)
		{
			wprintf(L"VesaInit: mode %04x: function 4F01 failed: AH = %x, AL = %x\n", 
				numbers[i], (uint8_t) (result >> 8), (uint8_t) result);
			memset(modes + i, 0, sizeof(vesa_mode_t));
			continue;
		}

		modes[i] = *(vesa_mode_t*) ((char*) vesa_data + vesa_data->mode_offset);
		if (VesaIsModeSuitable(modes + i))
			vesa_num_modes++;
		else
			modes[i].attributes = 0;
	}

	vesa_modes = malloc(sizeof(*vesa_modes) * vesa_num_modes);
	if (vesa_modes == NULL)
		goto error2;

	j = 0;
	for (i = 0; i < num_modes; i++)
	{
		if (modes[i].attributes != 0)
		{
			vesa_modes[j].mode.cookie = 0x4000 | numbers[i];
			vesa_modes[j].mode.width = modes[i].width;
			vesa_modes[j].mode.height = modes[i].height;
			vesa_modes[j].mode.bitsPerPixel = modes[i].bits_per_pixel;
			vesa_modes[j].mode.bytesPerLine = modes[i].bytes_per_line;
			vesa_modes[j].mode.accel = BPP8_ACCEL;

			if (modes[i].attributes & 16)
				vesa_modes[j].mode.flags = VIDEO_MODE_GRAPHICS;
			else
				vesa_modes[j].mode.flags = VIDEO_MODE_TEXT;

			swprintf(fb_name + 8, L"%08x", modes[i].linear_memory_phys);
			wprintf(L"%ux%ux%x @ %s\n", 
				vesa_modes[j].mode.width, 
				vesa_modes[j].mode.height, 
				vesa_modes[j].mode.bitsPerPixel, fb_name);

			for (k = 0; k < j; k++)
				if (wcscmp(fb_name, vesa_modes[k].mode.framebuffer) == 0)
					break;

			if (k >= j)
			{
				void *base;
				base = VmmMap(PAGE_ALIGN_UP(info->memory_size * 65536) / PAGE_SIZE, 
					NULL, 
					(void*) modes[i].linear_memory_phys,
					NULL, 
					VM_AREA_MAP, 
					VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
				VmmShare(base, fb_name);

				video_base = sbrk_virtual(info->memory_size * 65536 + PAGE_SIZE * 2);

				vesa_modes[j].base = video_base + PAGE_SIZE;
				MemMapRange(vesa_modes[j].base, 
					modes[i].linear_memory_phys, 
					(uint8_t*) vesa_modes[j].base + info->memory_size * 65536,
					PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);
				wprintf(L"\tallocated frame buffer at %p = %p, %u bytes\n", 
					base, vesa_modes[j].base, info->memory_size * 65536);
			}
			else
				vesa_modes[j].base = vesa_modes[k].base;

			wcscpy(vesa_modes[j].mode.framebuffer, fb_name);
			j++;
		}
	}

	assert(j == vesa_num_modes);
	free(modes);

	*add_device = VesaAddDevice;
	return true;

error2:
	vesa_num_modes = 0;
error1:
	free(modes);
error0:
	VesaCleanup();
	return false;
}
