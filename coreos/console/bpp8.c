/* $Id$ */

#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>

#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "internal.h"


static uint8_t *mem, /**buffer_mem, */*video_mem;
static unsigned char_width = 64, char_height = 64, font_height = 64;
static videomode_t *mode;
static handle_t vidmem;
static font_t *font;

extern uint8_t font8x8[];

static const uint32_t colours[16] =
{
    0x000000,
    0x800000,
    0x008000,
    0x808000,
    0x000080,
    0x800080,
    0x008080,
    0xC0C0C0,
    0x808080,
    0xFF0000,
    0x00FF00,
    0xFFFF00,
    0x0000FF,
    0xFF00FF,
    0x00FFFF,
    0xFFFFFF,
};


static uint8_t Con8RgbToIndex(uint32_t rgb)
{
    uint8_t r, g, b;
    r = COLOUR_RED(rgb);
    g = COLOUR_GREEN(rgb);
    b = COLOUR_BLUE(rgb);
    r = (r * 6) / 256;
    g = (g * 6) / 256;
    b = (b * 6) / 256;
    return 40 + r * 36 + g * 6 + b;
}


static uint8_t Con8GetBackgroundIndex(console_t *console)
{
    return Con8RgbToIndex(colours[((console->attribs >> 4) & 0x0f)]);
}


static void Con8Unload(void)
{
	FontDelete(font);
	VmmFree(video_mem);
    HndClose(vidmem);
}


static bool Con8CreateBuffer(void **buffer)
{
	return true;
}


static void Con8FreeBuffer(void *buffer)
{
}


/*static void Con8WriteChars(console_t *console, 
						   unsigned x, unsigned y, const wchar_t *str, 
						   size_t length, uint8_t fg, uint8_t bg)
{
    unsigned ax, ay;
    uint8_t *line, mask, *data;
    char *mb, *mb_ptr;

	x = (x * char_width) + console->rect.left;
	y = (y * char_height) + console->rect.top;

	mb_ptr = mb = malloc(length + 1);
    wcsto437(mb, str, length);
	fg = Con8RgbToIndex(colours[fg & 0x0f]);
    bg = Con8RgbToIndex(colours[bg & 0x0f]);

	while (length > 0)
	{
		data = font8x8 + font_height * (uint8_t) *mb_ptr;
		for (ay = 0; ay < char_height; ay++)
		{
			line = (mem + (y + ay) * mode->bytesPerLine) + x;
			ax = char_width;
			for (mask = 1; ax > 0; mask <<= 1)
			{
				ax--;
				if ((data[ay] & mask) != 0)
					line[ax] = fg;
				else
					line[ax] = bg;
			}
		}

		x += char_width;
		length--;
		mb_ptr++;
	}

	free(mb);
}*/


static void Con8WriteChars(console_t *console, 
						   unsigned x, unsigned y, const wchar_t *str, 
						   size_t length, uint8_t fg, uint8_t bg)
{
	x = (x * char_width) + console->rect.left;
	y = (y * char_height) + console->rect.top;
}


static void Con8DrawCursor(console_t *console, bool do_draw)
{
    unsigned x, y;
    uint8_t *line;

    if (do_draw != console->cursor_visible)
    {
        line = (video_mem + 
            (console->rect.top + (console->y + 1) * char_height - 2) 
                * mode->bytesPerLine) + 
            console->rect.left + console->x * char_width;

        for (y = char_height - 2; y < char_height; y++)
        {
            for (x = 0; x < char_width; x++)
                line[x] ^= 15;
            line += mode->bytesPerLine;
        }

        console->cursor_visible = do_draw;
    }
}


static void Con8UpdateSize(console_t *console)
{
	console->width = 
        (console->rect.right - console->rect.left) / char_width;
    console->height = 
        (console->rect.bottom - console->rect.top) / char_height;
}


static void Con8Scroll(console_t *console)
{
    unsigned y, line;
    uint8_t *src, *dest, index;

	y = console->y;
    while (y >= console->height)
    {
        dest = mem + console->rect.top * mode->bytesPerLine + console->rect.left;
        src = dest + font_height * mode->bytesPerLine;
        for (line = console->rect.top; line < console->rect.bottom - font_height; line++)
        {
            memmove(dest, src, console->rect.right - console->rect.left);
            dest += mode->bytesPerLine;
            src += mode->bytesPerLine;
        }

		index = Con8GetBackgroundIndex(console);
        for (; line < console->rect.bottom; line++)
        {
            memset(dest, 
				index,
                console->rect.right - console->rect.left);
            dest += mode->bytesPerLine;
        }

        y--;
    }
}


static void Con8UpdateCursor(console_t *console)
{
}


static void Con8Clear(console_t *console)
{
    unsigned y;
    uint8_t *line;

    line = mem + console->rect.top * mode->bytesPerLine;
    for (y = console->rect.top; y < console->rect.bottom; y++)
    {
        memset(line,
            Con8GetBackgroundIndex(console),
            console->rect.right - console->rect.left);

        line += mode->bytesPerLine;
    }
}


static void Con8ClearEol(console_t *console)
{
    unsigned y, width;
    uint8_t *line, index;

    line = (mem + (console->rect.top + console->y * char_height) * mode->bytesPerLine) + 
        console->rect.left + console->x * char_width;
    width = console->rect.right - console->rect.left - console->x * char_width;
	index = Con8GetBackgroundIndex(console);
    for (y = 0; y < char_height; y++)
    {
        memset(line, index, width);
        line += mode->bytesPerLine;
    }
}


static void Con8Focus(console_t *console)
{
    /*unsigned offset, y;

    offset = console->rect.top * mode->bytesPerLine + console->rect.left;
    for (y = console->rect.top; y < console->rect.bottom; y++)
    {
        memcpy(video_mem + offset, buffer_mem + offset, 
            console->rect.right - console->rect.left);
        offset += mode->bytesPerLine;
    }*/
}


static void Con8Blur(console_t *console)
{
}


static const buffer_vtbl_t con8_vtbl =
{
	Con8Unload,
	Con8CreateBuffer,
	Con8FreeBuffer,
	Con8WriteChars,
	Con8DrawCursor,
	Con8UpdateSize,
	Con8Scroll,
	Con8UpdateCursor,
	Con8Clear,
	Con8ClearEol,
	Con8Focus,
	Con8Blur,
};


const buffer_vtbl_t *Con8Init(handle_t thevid, videomode_t *themode)
{
	mode = themode;

	font = FontLoad(L"/Mobius/texm.ttf", 6 * 64);

	vidmem = HndOpen(mode->framebuffer);
    video_mem = VmmMapSharedArea(vidmem, NULL, 
        VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
    //buffer_mem = malloc(mode->bytesPerLine * mode->height);
    //mem = buffer_mem;
	mem = video_mem;

	return &con8_vtbl;
}
