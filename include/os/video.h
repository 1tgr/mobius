/* $Id: video.h,v 1.2 2002/03/07 15:51:51 pavlovskii Exp $ */

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

typedef struct vid_text_t vid_text_t;
struct vid_text_t
{
    const void *buffer;
    size_t length;
    int x, y;
    colour_t foreColour, backColour;
};

typedef struct vid_palette_t vid_palette_t;
struct vid_palette_t
{
    const void *entries;
    size_t length;
    uint32_t first_index;
    uint32_t reserved;
};

typedef struct videomode_t videomode_t;
struct videomode_t
{
    uint32_t width, height;
    uint8_t bitsPerPixel;
    uint32_t bytesPerLine;
    addr_t cookie;
};

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

/*#define VID_FILLRECT	  REQUEST_CODE(1, 0, 'v', 'f')
#define VID_HLINE	 REQUEST_CODE(1, 0, 'v', 'h')
#define VID_VLINE	 REQUEST_CODE(1, 0, 'v', 'v')
#define VID_LINE	REQUEST_CODE(1, 0, 'v', 'l')
#define VID_PUTPIXEL	REQUEST_CODE(1, 0, 'v', 'p')
#define VID_GETPIXEL	REQUEST_CODE(1, 0, 'v', 'g')*/

#define VID_DRAW	    REQUEST_CODE(1, 0, 'v', 'd')
#define VID_FILLPOLYGON	    REQUEST_CODE(1, 0, 'v', 'f')
#define VID_SETMODE	    REQUEST_CODE(0, 0, 'v', 'm')
#define VID_TEXTOUT	    REQUEST_CODE(0, 0, 'v', 't')
#define VID_STOREPALETTE    REQUEST_CODE(1, 0, 'v', 'P')
#define VID_LOADPALETTE     REQUEST_CODE(1, 0, 'v', 'Q')

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
    videomode_t vid_setmode;
    vid_palette_t vid_storepalette;
    vid_text_t vid_textout;

    struct
    {
	vid_shape_t *shapes;
	size_t length;
	uint64_t reserved;
    } vid_draw;

    struct
    {
	point_t *points;
	size_t length;
	colour_t colour;
	uint32_t reserved;
    } vid_fillpolygon;
};

#ifdef __cplusplus
}
#endif

#endif