/* $Id$ */

#include "common.h"
#include "internal.h"

gmgr_surface_t *GmgrCreateDeviceSurface(handle_t device, 
										const videomode_t *mode)
{
	gmgr_surface_t *surf;
	handle_t memory;

	surf = malloc(sizeof(*surf));
	if (surf == NULL)
		return NULL;

	surf->refs = 1;
	surf->device = device;
	surf->flags = GMGR_SURFACE_SHARED;
	surf->mode = *mode;

	if (mode->framebuffer[0] == '\0')
	{
		surf->base = NULL;

		if (!AccelCreateSurface(mode, device, &surf->surf))
		{
			_wdprintf(L"GmgrCreateDeviceSurface: AccelCreateSurface failed\n");
			free(surf);
			return NULL;
		}
	}
	else
	{
		memory = HndOpen(mode->framebuffer);
		surf->base = VmmMapSharedArea(memory,
			NULL,
			VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
		HndClose(memory);

		if (!FramebufCreateSurface(mode, surf->base, &surf->surf))
		{
			_wdprintf(L"GmgrCreateDeviceSurface: FramebufCreateSurface failed\n");
			VmmFree(surf->base);
			free(surf);
			return NULL;
		}
	}

	return surf;
}


gmgr_surface_t *GmgrCreateMemorySurface(const videomode_t *mode, uint32_t flags)
{
	gmgr_surface_t *surf;

	surf = malloc(sizeof(*surf));
	if (surf == NULL)
		return NULL;

	surf->device = NULL;
	surf->flags = flags;
	surf->mode = *mode;
	surf->base = VmmAlloc(PAGE_ALIGN_UP(mode->bytesPerLine * mode->height) / PAGE_SIZE,
		NULL,
		VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
	if (surf->base == NULL)
	{
		free(surf);
		return NULL;
	}

	if (!FramebufCreateSurface(mode, surf->base, &surf->surf))
	{
		VmmFree(surf->base);
		free(surf);
		return NULL;
	}

	return surf;
}


gmgr_surface_t *GmgrCopySurface(gmgr_surface_t *surf)
{
	SysIncrement(&surf->refs);
	return surf;
}


void GmgrCloseSurface(gmgr_surface_t *surf)
{
	if (SysDecrementAndTest(&surf->refs))
	{
		surf->surf.vtbl->SurfDeleteCookie(&surf->surf);

		if (surf->base != NULL)
		{
			if (surf->flags & GMGR_SURFACE_SHARED)
				VmmFree(surf->base);
			else
				free(surf->base);
		}

		if (surf->device != 0)
			HndClose(surf->device);

		free(surf);
	}
}
