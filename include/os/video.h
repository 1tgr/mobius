/* $Id: video.h,v 1.3 2003/06/22 15:50:13 pavlovskii Exp $ */

#ifndef __OS_VIDEO_H
#define __OS_VIDEO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <os/device.h>

typedef uint32_t colour_t;
#define color_t colour_t;

#define COLOUR_RED(c)           (((c) >> 16) & 0xff)
#define COLOUR_GREEN(c)         (((c) >> 8) & 0xff)
#define COLOUR_BLUE(c)          ((c) & 0xff)
#define MAKE_COLOUR(r, g, b)    (((b) & 0xff) | (((g) & 0xff) << 8) | (((r) & 0xff) << 16))

typedef struct rect_t rect_t;
struct rect_t
{
    int left, top, right, bottom;
};

typedef struct point_t point_t;
struct point_t
{
    int x, y;
};

/*typedef struct clip_t clip_t;
struct clip_t
{
    unsigned num_rects;
    rect_t *rects;
};*/

typedef struct vid_rect_t vid_rect_t;
struct vid_rect_t
{
    rect_t rect;
    colour_t colour;
};

typedef struct vid_line_t vid_line_t;
struct vid_line_t
{
    point_t a, b;
    colour_t colour;
};

typedef struct vid_pixel_t vid_pixel_t;
struct vid_pixel_t
{
    point_t point;
    colour_t colour;
};

/*typedef struct vid_text_t vid_text_t;
struct vid_text_t
{
    clip_t clip;
    const void *buffer;
    size_t length;
    rect_t rect;
    colour_t foreColour, backColour;
};*/

typedef struct vid_palette_t vid_palette_t;
struct vid_palette_t
{
    const void *entries;
    size_t length;
    uint32_t first_index;
    uint32_t reserved;
};

typedef struct vid_blt_t vid_blt_t;
struct vid_blt_t
{
	union
	{
		struct
		{
			void *buffer;
			int pitch;
		} memory;
		rect_t rect;
	} src, dest;
};

typedef struct videomode_t videomode_t;
struct videomode_t
{
    addr_t cookie;
    uint32_t width, height;
    uint8_t bitsPerPixel;
    uint32_t bytesPerLine;
    uint32_t flags;
	uint32_t accel;
    wchar_t framebuffer[20];
};

#define VIDEO_MODE_GRAPHICS					0
#define VIDEO_MODE_TEXT						1

#define VIDEO_ACCEL_FILLRECT				0x01
#define VIDEO_ACCEL_LINE					0x02
#define VIDEO_ACCEL_BLT_SCREEN_TO_SCREEN	0x04
#define VIDEO_ACCEL_BLT_SCREEN_TO_MEMORY	0x08
#define VIDEO_ACCEL_BLT_MEMORY_TO_SCREEN	0x10
#define VIDEO_ACCEL_CURSOR					0x20

typedef struct
{
    uint16_t First, Last;
    uint8_t Height;
    uint8_t *Bitmaps;
} vga_font_t;

typedef struct rgb_t rgb_t;
struct rgb_t
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t reserved;
};

//#define VID_FILLPOLYGON				REQUEST_CODE(1, 0, 'v', 'f')
#define VID_SETMODE					REQUEST_CODE(0, 0, 'v', 'm')
#define VID_GETMODE					REQUEST_CODE(0, 0, 'v', 'M')
#define VID_STOREPALETTE			REQUEST_CODE(1, 0, 'v', 'P')
#define VID_LOADPALETTE				REQUEST_CODE(1, 0, 'v', 'Q')
#define VID_MOVECURSOR				REQUEST_CODE(0, 0, 'v', 'c')

#define VID_DRAW					REQUEST_CODE(1, 0, 'v', 'd')
#define VID_BLT_SCREEN_TO_SCREEN	REQUEST_CODE(0, 0, 'v', '1')
#define VID_BLT_SCREEN_TO_MEMORY	REQUEST_CODE(0, 0, 'v', '2')
#define VID_BLT_MEMORY_TO_SCREEN	REQUEST_CODE(0, 0, 'v', '3')

enum
{
    VID_SHAPE_FILLRECT,
    VID_SHAPE_HLINE,
    VID_SHAPE_VLINE,
    VID_SHAPE_LINE,
    VID_SHAPE_PUTPIXEL,
    VID_SHAPE_GETPIXEL
};

typedef struct vid_shape_t vid_shape_t;
struct vid_shape_t
{
    uint32_t shape;
    union
    {
        vid_rect_t rect;
        vid_line_t line;
        vid_pixel_t pix;
    } s;
};

typedef union params_vid_t params_vid_t;
union params_vid_t
{
    videomode_t vid_setmode, vid_getmode;
    vid_palette_t vid_storepalette;
    point_t vid_movecursor;
	vid_blt_t vid_blt;

    struct
    {
        vid_shape_t *shapes;
        size_t length;
        uint64_t reserved;
    } vid_draw;

    /*struct
    {
        point_t *points;
        size_t length;
        colour_t colour;
        uint32_t reserved;
    } vid_fillpolygon;*/
};

#ifdef __cplusplus
}
#endif

#endif
