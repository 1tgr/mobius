/* $Id$ */

#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>

#include "internal.h"


#define CHAR_WIDTH		8
#define CHAR_HEIGHT		8
#define FONT_HEIGHT		8
#define CURSOR_HEIGHT	2

static handle_t vid;
static videomode_t *mode;

extern uint8_t font8x8[];


static bool Con4BltScreenToScreen(const rect_t *dest, const rect_t *src)
{
	params_vid_t params;
	params.vid_blt.src.rect = *src;
	params.vid_blt.dest.rect = *dest;
	return FsRequestSync(vid, VID_BLT_SCREEN_TO_SCREEN, &params, sizeof(params), NULL);
}


static bool Con4BltScreenToMemory(void *dest_buffer, int dest_pitch, const rect_t *src)
{
	params_vid_t params;
	params.vid_blt.src.rect = *src;
	params.vid_blt.dest.memory.buffer = dest_buffer;
	params.vid_blt.dest.memory.pitch = dest_pitch;
	return FsRequestSync(vid, VID_BLT_SCREEN_TO_MEMORY, &params, sizeof(params), NULL);
}


static bool Con4BltMemoryToScreen(const rect_t *dest, void *src_buffer, int src_pitch)
{
	params_vid_t params;
	params.vid_blt.src.memory.buffer = src_buffer;
	params.vid_blt.src.memory.pitch = src_pitch;
	params.vid_blt.dest.rect = *dest;
	return FsRequestSync(vid, VID_BLT_MEMORY_TO_SCREEN, &params, sizeof(params), NULL);
}


static bool Con4FillRect(const rect_t *rect, uint8_t colour)
{
	params_vid_t params;
	vid_shape_t shape;

	shape.shape = VID_SHAPE_FILLRECT;
	shape.s.rect.rect = *rect;
	shape.s.rect.colour = colour;

	params.vid_draw.shapes = &shape;
	params.vid_draw.length = sizeof(shape);
	return FsRequestSync(vid, VID_DRAW, &params, sizeof(params), NULL);
}


static void Con4Unload(void)
{
}


static bool Con4CreateBuffer(void **buffer)
{
	*buffer = NULL;
	return true;
}


static void Con4FreeBuffer(void *buffer)
{
}


static void Con4WriteChars(console_t *console, 
						   unsigned x, unsigned y, const wchar_t *str, 
						   size_t length, uint8_t fg, uint8_t bg)
{
	unsigned i, ay;
    uint8_t *line, mask, *data, *bitmap, *bitmap_ptr;
	int pitch;
    char *mb, *mb_ptr;
	rect_t rect;

	assert(!console->cursor_visible);
	mb_ptr = mb = malloc(length + 1);
    wcsto437(mb, str, length);
	pitch = (length * CHAR_WIDTH) / 2;
	bitmap_ptr = bitmap = malloc(pitch * CHAR_HEIGHT);

	rect.left = console->rect.left + x * CHAR_WIDTH;
	rect.top = console->rect.top + y * CHAR_HEIGHT;
	rect.right = rect.left + length * CHAR_WIDTH;
	rect.bottom = rect.top + CHAR_HEIGHT;

	while (length > 0)
	{
		data = font8x8 + FONT_HEIGHT * (uint8_t) *mb_ptr;
		line = bitmap_ptr;

		for (ay = 0; ay < CHAR_HEIGHT; ay++)
		{
			mask = 1;
			i = CHAR_WIDTH / 2;
			while (i > 0)
			{
				i--;

				if ((data[ay] & mask) != 0)
					line[i] = fg;
				else
					line[i] = bg;
				mask <<= 1;

				if ((data[ay] & mask) != 0)
					line[i] |= fg << 4;
				else
					line[i] |= bg << 4;
				mask <<= 1;
			}

			line += pitch;
		}

		bitmap_ptr += CHAR_WIDTH / 2;
		length--;
		mb_ptr++;
	}

	Con4BltMemoryToScreen(&rect, bitmap, pitch);

	free(mb);
	free(bitmap);
}


static void Con4DrawCursor(console_t *console, bool do_draw)
{
	uint8_t block[CURSOR_HEIGHT * CHAR_WIDTH / 2];
	rect_t rect;
	unsigned i;

	if (console->cursor_visible != do_draw)
	{
		rect.left = console->rect.left + console->x * CHAR_WIDTH;
		rect.top = console->rect.top + (console->y + 1) * CHAR_HEIGHT - CURSOR_HEIGHT;
		rect.right = rect.left + CHAR_WIDTH;
		rect.bottom = rect.top + CURSOR_HEIGHT;

		Con4BltScreenToMemory(block, CHAR_WIDTH / 2, &rect);
		for (i = 0; i < _countof(block); i++)
			block[i] = ~block[i];
		Con4BltMemoryToScreen(&rect, block, CHAR_WIDTH / 2);

		console->cursor_visible = do_draw;
	}
}


static void Con4UpdateSize(console_t *console)
{
	console->width = 
        (console->rect.right - console->rect.left) / CHAR_WIDTH;
    console->height = 
        (console->rect.bottom - console->rect.top) / CHAR_HEIGHT;
}


static void Con4Scroll(console_t *console)
{
    unsigned lines;
	rect_t src, dest;

	assert(!console->cursor_visible);
	if (console->y > console->height - 1)
	{
		lines = console->y - (console->height - 1);

		src = dest = console->rect;
		src.top += lines * CHAR_HEIGHT;
		dest.bottom -= lines * CHAR_HEIGHT;
        Con4BltScreenToScreen(&dest, &src);

		src.top = dest.bottom;
		Con4FillRect(&src, console->attribs >> 4);
    }
}


static void Con4UpdateCursor(console_t *console)
{
}


static void Con4Clear(console_t *console)
{
	assert(!console->cursor_visible);
    Con4FillRect(&console->rect, console->attribs >> 4);
}


static void Con4ClearEol(console_t *console)
{
    rect_t rect;

	assert(!console->cursor_visible);
    rect.left = console->rect.left + console->x * CHAR_WIDTH;
	rect.top = console->rect.top + console->y * CHAR_HEIGHT;
	rect.right = console->rect.right;
	rect.bottom = rect.top + CHAR_HEIGHT;

	Con4FillRect(&rect, console->attribs >> 4);
}


static void Con4Focus(console_t *console)
{
}


static void Con4Blur(console_t *console)
{
}


static const buffer_vtbl_t con4_vtbl =
{
	Con4Unload,
	Con4CreateBuffer,
	Con4FreeBuffer,
	Con4WriteChars,
	Con4DrawCursor,
	Con4UpdateSize,
	Con4Scroll,
	Con4UpdateCursor,
	Con4Clear,
	Con4ClearEol,
	Con4Focus,
	Con4Blur,
};


const buffer_vtbl_t *Con4Init(handle_t thevid, videomode_t *themode)
{
	vid = thevid;
	mode = themode;
	return &con4_vtbl;
}
