/* $Id$ */

#include "common.h"
#include "internal.h"
#include <stddef.h>
#include <assert.h>

lmutex_t gmgr_draw;
lmutex_t gmgr_mux_gfxs;
gfx_t **gmgr_gfxs;
unsigned gmgr_num_gfxs;


gfx_t *GmgrLockGraphics(gfx_h hgfx)
{
    wnd_t *wnd;
    gfx_t *gfx;

    if (hgfx & 0x80000000)
    {
        wnd = WmgrLockWindow(hgfx & ~0x80000000);
        if (wnd == NULL)
		{
			_wdprintf(L"GmgrLockGraphics: invalid window handle %lx\n", hgfx & ~0x80000000);
			return NULL;
		}

        gfx = &wnd->gfx;
		WmgrUnlockWindow(wnd);
    }
    else
    {
		gfx = NULL;

		LmuxAcquire(&gmgr_mux_gfxs);
		if (hgfx >= gmgr_num_gfxs ||
			(gfx = gmgr_gfxs[hgfx]) == NULL)
		{
			_wdprintf(L"GmgrLockGraphics: invalid graphics handle %lx\n", hgfx);
			LmuxRelease(&gmgr_mux_gfxs);
			return NULL;
		}

		LmuxRelease(&gmgr_mux_gfxs);
    }

    LmuxAcquire(&gfx->mux);
    return gfx;
}

void GmgrUnlockGraphics(gfx_t *gfx)
{
	LmuxRelease(&gfx->mux);
}


void GmgrInitDefaultGraphics(gfx_t *gfx)
{
	gfx->surface = GmgrCopySurface(gmgr_screen);
    gfx->colour_fill = 0x000000;
    gfx->colour_pen = 0xffffff;
    LmuxInit(&gfx->mux);
	gfx->clip.rects = NULL;
	gfx->clip.num_rects = 0;
}


void GmgrClose(gfx_h hgfx)
{
	gfx_t *gfx;

	gfx = GmgrLockGraphics(hgfx);
	if (gfx == NULL)
		return;

	GmgrCloseSurface(gfx->surface);

	if (hgfx < 0x80000000)
	{
		gmgr_gfxs[hgfx] = NULL;
		free(gfx);
	}
}


void GmgrSetFillColour(gfx_h hgfx, colour_t clr)
{
    gfx_t *gfx;

    gfx = GmgrLockGraphics(hgfx);
    if (gfx == NULL)
        return;

    gfx->colour_fill = clr;
    GmgrUnlockGraphics(gfx);
}


void GmgrSetPenColour(gfx_h hgfx, colour_t clr)
{
    gfx_t *gfx;

    gfx = GmgrLockGraphics(hgfx);
    if (gfx == NULL)
        return;

    gfx->colour_pen = clr;
    GmgrUnlockGraphics(gfx);
}


void GmgrFlush(const void *ptr, size_t length)
{
    const uint8_t *buf, *top;
    const gfx_command_t *cmd;
    gfx_h gfx_referenced[8];
    unsigned num_referenced, i;

	WmgrShowCursor(false);
	LmuxAcquire(&gmgr_draw);

    buf = ptr;
    top = buf + length;
    num_referenced = 0;
	//_wdprintf(L"server: length = %u\n", length);
    while (length > 0)
    {
        cmd = (const gfx_command_t*) buf;
		//_wdprintf(L"command = %lu, size = %u, num_referenced = %u\n", 
            //cmd->command,
            //cmd->size, 
            //num_referenced);
        assert(cmd->size != 0);
        length -= cmd->size;
        buf += cmd->size;
        if (buf > top)
            goto finished;

        switch (cmd->command)
        {
        case GFX_CMD_NOP:
            break;

        case GFX_CMD_FILL_RECT:
            GmgrFillRect(cmd->hgfx, &cmd->u.fill_rect);
            break;

        case GFX_CMD_RECTANGLE:
            GmgrRectangle(cmd->hgfx, &cmd->u.fill_rect);
            break;

        case GFX_CMD_DRAW_TEXT:
			{
				size_t text_length;

				text_length = (wchar_t*) ((uint8_t*) cmd + cmd->size) - cmd->u.draw_text.str;
				while (text_length > 0 &&
					cmd->u.draw_text.str[text_length - 1] == '\0')
					text_length--;

				GmgrDrawText(cmd->hgfx, 
					&cmd->u.draw_text.rect, 
					cmd->u.draw_text.str, 
					text_length);
			}
            break;

        case GFX_CMD_SET_FILL_COLOUR:
            GmgrSetFillColour(cmd->hgfx, cmd->u.set_fill_colour);
            break;

        case GFX_CMD_SET_PEN_COLOUR:
            GmgrSetPenColour(cmd->hgfx, cmd->u.set_pen_colour);
            break;

        case GFX_CMD_LINE:
            GmgrLine(cmd->hgfx, cmd->u.line.x1, cmd->u.line.y1, cmd->u.line.x2, cmd->u.line.y2);
            break;

		case GFX_CMD_BLT:
			GmgrBlt(cmd->hgfx, &cmd->u.blt.dest, cmd->u.blt.hsrc, &cmd->u.blt.src);
			break;
        }

        for (i = 0; i < num_referenced; i++)
            if (gfx_referenced[i] == cmd->hgfx)
                break;

        if (i == num_referenced &&
            num_referenced < _countof(gfx_referenced))
            gfx_referenced[num_referenced++] = cmd->hgfx;
    }

finished:
    for (i = 0; i < num_referenced; i++)
        GmgrFinishedPainting(gfx_referenced[i]);

	LmuxRelease(&gmgr_draw);
	WmgrShowCursor(true);
}


gfx_h GmgrCreateBitmap(unsigned width, unsigned height, bitmap_desc_t *desc)
{
	gfx_t *gfx;
	gfx_h hgfx;
	videomode_t mode;
	handle_t shared;

	gfx = malloc(sizeof(gfx_t));
	if (gfx == NULL)
		return 0;

	mode = gmgr_screen->mode;
	mode.width = width;
	mode.height = height;
	mode.bytesPerLine = (mode.bitsPerPixel * mode.width) / 8;
	gfx->surface = GmgrCreateMemorySurface(&mode, 0);
	if (gfx->surface == NULL)
	{
		free(gfx);
		return 0;
	}

	GmgrInitDefaultGraphics(gfx);

	LmuxAcquire(&gmgr_mux_gfxs);
	if (gmgr_num_gfxs == 0)
	{
		hgfx = 1;
		gmgr_num_gfxs = 2;
		gmgr_gfxs = malloc(sizeof(*gmgr_gfxs) * gmgr_num_gfxs);
		gmgr_gfxs[0] = NULL;
	}
	else
	{
		hgfx = gmgr_num_gfxs;
		gmgr_num_gfxs++;
		gmgr_gfxs = realloc(gmgr_gfxs, sizeof(*gmgr_gfxs) * gmgr_num_gfxs);
	}

	gmgr_gfxs[hgfx] = gfx;
	LmuxRelease(&gmgr_mux_gfxs);

	desc->width = width;
	desc->height = height;
	desc->pitch = mode.bytesPerLine;
	desc->bits_per_pixel = mode.bitsPerPixel;
	swprintf(desc->area_name, L"_winmgr_%08x", hgfx);
	shared = VmmShare(gfx->surface->base, desc->area_name);
	HndClose(shared);

	return hgfx;
}
