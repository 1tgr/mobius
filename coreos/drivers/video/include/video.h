/* $Id: video.h,v 1.3 2003/06/22 15:43:38 pavlovskii Exp $ */

#ifndef __VIDEO_H
#define __VIDEO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/driver.h>
#include <os/video.h>

typedef struct surface_vtbl_t surface_vtbl_t;

typedef struct surface_t surface_t;
struct surface_t
{
	void *cookie;
	const surface_vtbl_t *vtbl;
};


struct surface_vtbl_t
{
	void (*SurfPutPixel)(surface_t *surf, int x, int y, colour_t c);
	colour_t (*SurfGetPixel)(surface_t *surf, int x, int y);
	void (*SurfHLine)(surface_t *surf, int x1, int x2, int y, colour_t c);
	void (*SurfVLine)(surface_t *surf, int x, int y1, int y2, colour_t c);
	void (*SurfLine)(surface_t *surf, int x1, int y1, int x2, int y2, colour_t d);
	void (*SurfFillRect)(surface_t *surf, int x1, int y1, int x2, int y2, colour_t c);
	void (*SurfBltScreenToScreen)(surface_t *surf, const rect_t *dest, const rect_t *src);
	void (*SurfBltScreenToMemory)(surface_t *surf, void *dest, int dest_pitch, const rect_t *src);
	void (*SurfBltMemoryToScreen)(surface_t *surf, const rect_t *dest, const void *src, int src_pitch);
};


typedef struct video_vtbl_t video_vtbl_t;
struct video_vtbl_t
{
	int  (*VidEnumModes)(void *cookie, unsigned index, videomode_t *mode);
	bool (*VidEnterMode)(void *cookie, videomode_t *mode, const surface_vtbl_t **surf_vtbl, void **surf_cookie);
	void (*VidLeaveMode)(void *cookie);
	void (*VidStorePalette)(void *cookie, const rgb_t *entries, unsigned first, unsigned count);
	void (*VidMoveCursor)(void *cookie, point_t pt);
};


#define VID_ENUM_CONTINUE	1
#define VID_ENUM_ERROR		0
#define VID_ENUM_STOP		-1

typedef bool (*VIDEO_ADD_DEVICE)(dev_config_t *cfg, 
								 const video_vtbl_t **vtbl, 
								 void **cookie);

bool	VideoEntry(VIDEO_ADD_DEVICE *add_device);

void	VidLockVga();
void	VidUnlockVga();
void	VidWriteVgaRegisters(const uint8_t *regs);
void	VidStoreVgaPalette(void *cookie, const rgb_t *entries, 
						   unsigned first, unsigned count);

void	SurfFillRect(surface_t *surf, int x1, int y1, int x2, int y2, colour_t c);
void	SurfHLine(surface_t *surf, int x1, int x2, int y, colour_t c);
void	SurfVLine(surface_t *surf, int x, int y1, int y2, colour_t c);
void	SurfLine(surface_t *surf, int x1, int y1, int x2, int y2, colour_t d);

const rgb_t *Bpp8GetPalette(void);
bool	Bpp8CreateSurface(const videomode_t *mode, void *base, 
						  const surface_vtbl_t **vtbl, void **cookie);

#define BPP8_ACCEL			(VIDEO_ACCEL_FILLRECT | VIDEO_ACCEL_LINE)

#ifdef __cplusplus
}
#endif

#endif
