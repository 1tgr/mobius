/* $Id$ */

#include <gui/winmgr.h>
#include <gui/ipc.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

uint8_t *gfx_commands;
unsigned gfx_commands_size, gfx_commands_alloc;

static void GfxQueueCommand(uint32_t command, uint32_t size, gfx_h hgfx, const gfx_command_t *cmd)
{
    gfx_command_t *c;

    gfx_commands_size += size;
    if (gfx_commands_size > gfx_commands_alloc)
    {
        gfx_commands_alloc = (gfx_commands_size + 4096) & -4096;
        gfx_commands = realloc(gfx_commands, gfx_commands_alloc);
    }

    c = (gfx_command_t*) (gfx_commands + gfx_commands_size - size);
    c->command = command;
    c->size = size;
    c->hgfx = hgfx;
    memcpy(&c->u, &cmd->u, size - offsetof(gfx_command_t, u));
}

void __declspec(dllexport) GfxFillRect(gfx_h hgfx, const rect_t *rect)
{
    gfx_command_t cmd;
    cmd.u.fill_rect = *rect;
    GfxQueueCommand(GFX_CMD_FILL_RECT, sizeof(cmd), hgfx, &cmd);
}

void __declspec(dllexport) GfxRectangle(gfx_h hgfx, const rect_t *rect)
{
    gfx_command_t cmd;
    cmd.u.rectangle = *rect;
    GfxQueueCommand(GFX_CMD_RECTANGLE, sizeof(cmd), hgfx, &cmd);
}

void __declspec(dllexport) GfxDrawText(gfx_h hgfx, const rect_t *rect, const wchar_t *str, size_t length)
{
    gfx_command_t *cmd;
	unsigned aligned_length;

	aligned_length = (length * sizeof(wchar_t) + 3) & -4;
	cmd = malloc(sizeof(gfx_command_t) + aligned_length);
    cmd->u.draw_text.rect = *rect;
    memcpy(cmd->u.draw_text.str, str, sizeof(wchar_t) * length);
	memset(cmd->u.draw_text.str + length, 0, aligned_length - sizeof(wchar_t) * length);
    GfxQueueCommand(GFX_CMD_DRAW_TEXT, 
		offsetof(gfx_command_t, u.draw_text.str) + aligned_length, 
		hgfx, 
		cmd);
    free(cmd);
}

void __declspec(dllexport) GfxSetFillColour(gfx_h hgfx, colour_t clr)
{
    gfx_command_t cmd;
    cmd.u.set_fill_colour = clr;
    GfxQueueCommand(GFX_CMD_SET_FILL_COLOUR, sizeof(cmd), hgfx, &cmd);
}

void __declspec(dllexport) GfxSetPenColour(gfx_h hgfx, colour_t clr)
{
    gfx_command_t cmd;
    cmd.u.set_pen_colour = clr;
    GfxQueueCommand(GFX_CMD_SET_PEN_COLOUR, sizeof(cmd), hgfx, &cmd);
}

void __declspec(dllexport) GfxLine(gfx_h hgfx, int x1, int y1, int x2, int y2)
{
    gfx_command_t cmd;
    cmd.u.line.x1 = x1;
    cmd.u.line.y1 = y1;
    cmd.u.line.x2 = x2;
    cmd.u.line.y2 = y2;
    GfxQueueCommand(GFX_CMD_LINE, sizeof(cmd), hgfx, &cmd);
}

void __declspec(dllexport) GfxBlt(gfx_h hdest, const rect_t *dest, gfx_h hsrc, const rect_t *src)
{
	gfx_command_t cmd;
    cmd.u.blt.hsrc = hsrc;
    cmd.u.blt.src = *src;
	cmd.u.blt.dest = *dest;
    GfxQueueCommand(GFX_CMD_BLT, sizeof(cmd), hdest, &cmd);
}

void __declspec(dllexport) GfxFlush(void)
{
	IpcCallExtra(NULL, SYS_GfxFlush, NULL, gfx_commands, gfx_commands_size);
    gfx_commands_size = 0;
}

gfx_h __declspec(dllexport) GfxCreateBitmap(unsigned width, unsigned height, bitmap_desc_t *desc)
{
	wnd_params_t params;

	params.gfx_create_bitmap.width = width;
	params.gfx_create_bitmap.height = height;
	if (!IpcCall(NULL, SYS_GfxCreateBitmap, &params))
		return 0;

	if (IpcReceive(NULL, &params))
    {
        *desc = params.gfx_create_bitmap_reply.desc;
		return params.gfx_create_bitmap_reply.gfx;
	}
	else
		return 0;
}

void __declspec(dllexport) GfxClose(gfx_h hgfx)
{
	wnd_params_t params;
	params.gfx_close.gfx = hgfx;
	IpcCall(NULL, SYS_GfxClose, &params);
}


void __declspec(dllexport) GfxGetTextSize(gfx_h hgfx, const wchar_t *str, size_t length, point_t *size)
{
	wnd_params_t params;
	params.gfx_get_text_size.gfx = hgfx;

	if (!IpcCallExtra(NULL, SYS_GfxGetTextSize, &params, str, length * sizeof(wchar_t)) ||
		!IpcReceive(NULL, &params))
	{
		size->x = size->y = 0;
		return;
	}

	*size = params.gfx_get_text_size_reply.size;
}
