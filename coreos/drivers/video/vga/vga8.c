/* $Id: vga8.c,v 1.2 2003/06/05 21:59:53 pavlovskii Exp $ */

#include <kernel/kernel.h>

#include "video.h"
#include "bpp8.h"

extern uint8_t *const vga_base_global;

bool Vga8Init(void)
{
	return true;
}


bool Vga8EnterMode(void *cookie, videomode_t *mode, const uint8_t *regs, 
				   const surface_vtbl_t **surf_vtbl, void **surf_cookie)
{
    VidWriteVgaRegisters(regs);
	VidStoreVgaPalette(cookie, Bpp8GetPalette(), 0, 256);
	return Bpp8CreateSurface(mode, vga_base_global, surf_vtbl, surf_cookie);
}
