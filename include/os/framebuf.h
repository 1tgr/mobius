/* $Id$ */
#ifndef __OS_FRAMEBUF_H
#define __OS_FRAMEBUF_H

#include <os/video.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct surface_vtbl_t surface_vtbl_t;

typedef struct surface_t surface_t;
struct surface_t
{
	void *cookie;
	const surface_vtbl_t *vtbl;
};

typedef struct bitmapdesc_t bitmapdesc_t;
struct bitmapdesc_t
{
	unsigned width;
	unsigned height;
	unsigned bits_per_pixel;
	int pitch;
	void *data;
	rgb_t *palette;
};


struct surface_vtbl_t
{
	void (*SurfDeleteCookie)(surface_t *surf);
	void (*SurfPutPixel)(surface_t *surf, int x, int y, colour_t c);
	colour_t (*SurfGetPixel)(surface_t *surf, int x, int y);
	void (*SurfHLine)(surface_t *surf, int x1, int x2, int y, colour_t c);
	void (*SurfVLine)(surface_t *surf, int x, int y1, int y2, colour_t c);
	void (*SurfLine)(surface_t *surf, int x1, int y1, int x2, int y2, colour_t d);
	void (*SurfFillRect)(surface_t *surf, int x1, int y1, int x2, int y2, colour_t c);
	void (*SurfBltScreenToScreen)(surface_t *surf, const rect_t *dest, const rect_t *src);
	void (*SurfBltScreenToMemory)(surface_t *surf, void *dest, int dest_pitch, const rect_t *src);
	void (*SurfBltMemoryToScreen)(surface_t *surf, const rect_t *dest, const void *src, int src_pitch);
	void (*SurfBltMemoryToMemory)(surface_t *surf, 
		bitmapdesc_t *dest, const rect_t *dest_rect,
		const bitmapdesc_t *src, const rect_t *src_rect);
	void (*SurfFlush)(surface_t *surf);
	status_t (*SurfConvertBitmap)(surface_t *surf, 
		void **dest, int *dest_pitch, const bitmapdesc_t *src);
};

void SurfFillRect(surface_t *surf, int x1, int y1, int x2, int y2, colour_t c);
void SurfHLine(surface_t *surf, int x1, int x2, int y, colour_t c);
void SurfVLine(surface_t *surf, int x, int y1, int y2, colour_t c);
void SurfLine(surface_t *surf, int x1, int y1, int x2, int y2, colour_t d);
void SurfFlush(surface_t *surf);

status_t Bpp4ConvertBitmap(surface_t *surf, 
	void **dest, int *dest_pitch, const bitmapdesc_t *src);
status_t Bpp8ConvertBitmap(surface_t *surf, 
	void **dest, int *dest_pitch, const bitmapdesc_t *src);

void Bpp4BltMemoryToMemory(surface_t *surf, 
	bitmapdesc_t *dest, const rect_t *dest_rect,
	const bitmapdesc_t *src, const rect_t *src_rect);
void Bpp8BltMemoryToMemory(surface_t *surf, 
	bitmapdesc_t *dest, const rect_t *dest_rect,
	const bitmapdesc_t *src, const rect_t *src_rect);

bool FramebufCreateSurface(const videomode_t *mode, void *base, surface_t *surf);
bool AccelCreateSurface(const videomode_t *mode, handle_t device, surface_t *surf);

typedef struct font_t font_t;

#define FB_FONT_SMOOTH	0
#define FB_FONT_MONO	1

bool	FontInit(void);
font_t *FontLoad(const wchar_t *filename, unsigned size_points_64, uint32_t options);
void	FontDelete(font_t *font);
void	FontGetMaxSize(font_t *font, unsigned *width, unsigned *height);
void	FontDrawText(font_t *font, surface_t *surf, 
					 unsigned x, unsigned y, 
					 const wchar_t *str, size_t length, 
					 colour_t fg, colour_t bg);
void	FontGetTextSize(font_t *font, const wchar_t *str, size_t length, point_t *size);
uint32_t FontGetOptions(font_t *font);
void	FontSetOptions(font_t *font, uint32_t options);

#ifdef KERNEL
#define fbprintf wprintf
#else
#define fbprintf _wdprintf
#endif

#ifdef __cplusplus
}
#endif

#endif
