#ifndef INTERNAL_H__
#define INTERNAL_H__

#include <os/video.h>
#include <os/defs.h>
#include <os/framebuf.h>

typedef struct console_t console_t;
struct console_t
{
    handle_t client;
	lmutex_t lmux;
	unsigned width, height;
    unsigned x, y;
	unsigned char_width, char_height;
	font_t *font;
    void *cookie;
    rect_t rect;
    wchar_t *buf_chars;
	bool cursor_visible;
	colour_t fg_colour;
	colour_t bg_colour;
	colour_t default_fg_colour;
	colour_t default_bg_colour;
    unsigned save_x, save_y;
    unsigned escape, esc[3], esc_param;
	unsigned str_x, str_y;
};

#endif
