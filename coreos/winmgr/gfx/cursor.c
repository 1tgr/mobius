/* $Id$ */

#include "internal.h"

#include <stdlib.h>
#include <errno.h>
#include <wchar.h>

enum
{
	CURSOR_WIDTH = 16,
	CURSOR_HEIGHT = 16
};

static uint16_t bits[(CURSOR_WIDTH * CURSOR_HEIGHT) / 16] =
{
	0xffff,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0x0180,
	0xffff,
};

static bitmapdesc_t combined, image, under;

static void set_cursor_rect(rect_t *rect, int x, int y)
{
	GmgrToScreenPoint(&rect->left, &rect->top, x, y);
	rect->right = rect->left + CURSOR_WIDTH;
	rect->bottom = rect->top + CURSOR_HEIGHT;

	if (rect->right > gmgr_screen->mode.width)
		rect->right = gmgr_screen->mode.width;
	if (rect->bottom > gmgr_screen->mode.height)
		rect->bottom = gmgr_screen->mode.height;
}


void GmgrDrawCursor(int x, int y, bool draw)
{
	rect_t rect;

	set_cursor_rect(&rect, x, y);
	if (draw)
	{
		gmgr_screen->surf.vtbl->SurfBltScreenToMemory(&gmgr_screen->surf,
			under.data, under.pitch,
			&rect);
		gmgr_screen->surf.vtbl->SurfBltMemoryToScreen(&gmgr_screen->surf,
			&rect,
			image.data, image.pitch);
	}
	else
		gmgr_screen->surf.vtbl->SurfBltMemoryToScreen(&gmgr_screen->surf,
			&rect,
			under.data, under.pitch);
}


void GmgrMoveCursor(int from_x, int from_y, int to_x, int to_y)
{
	static const rect_t rect_cursor = { 0, 0, CURSOR_WIDTH, CURSOR_HEIGHT };
	rect_t from, to, both;

	set_cursor_rect(&from, from_x, from_y);
	set_cursor_rect(&to, to_x, to_y);

	if (RectTestOverlap(&from, &to))
	{
		RectUnion(&both, &from, &to);
		RectOffset(&from, -both.left, -both.top);
		RectOffset(&to, -both.left, -both.top);

		/* Capture what's on the screen now to combined */
		gmgr_screen->surf.vtbl->SurfBltScreenToMemory(&gmgr_screen->surf,
			combined.data, combined.pitch,
			&both);

		/* Erase the cursor image from combined */
		gmgr_screen->surf.vtbl->SurfBltMemoryToMemory(&gmgr_screen->surf,
			&combined, &from,
			&under, &rect_cursor);

		/* Capture what's at the new position to under */
		gmgr_screen->surf.vtbl->SurfBltMemoryToMemory(&gmgr_screen->surf,
			&under, &rect_cursor,
			&combined, &to);

		/* Draw the cursor image to combined */
		gmgr_screen->surf.vtbl->SurfBltMemoryToMemory(&gmgr_screen->surf,
			&combined, &to,
			&image, &rect_cursor);

		/* Draw the combined area to the screen */
		gmgr_screen->surf.vtbl->SurfBltMemoryToScreen(&gmgr_screen->surf,
			&both,
			combined.data, combined.pitch);
	}
	else
	{
		GmgrDrawCursor(from_x, from_y, false);
		GmgrDrawCursor(to_x, to_y, true);
	}
}


static void GmgrCursorCleanup(void)
{
	free(image.data);
	free(under.data);
	free(combined.data);

	image.data = NULL;
	under.data = NULL;
	combined.data = NULL;
}


bool GmgrInitCursor(void)
{
	static rgb_t palette2[] =
	{
		{ 0x00, 0x00, 0x00, 0 },
		{ 0xff, 0xff, 0xff, 0 },
	};

	bitmapdesc_t desc;
	status_t status;

	atexit(GmgrCursorCleanup);

	/*
	 * Set up a bitmap descriptor for:
	 *	- Cursor plus background (combined)
	 *	- Cursor image (image)
	 *	- Cursor background (under)
	 *
	 *	combined is big enough to hold image and under 
	 *		when they just overlap.
	 *	image holds the actual mouse pointer definition.
	 *	under holds what was underneath the cursor before we drew it.
	 */

	/* Create cursor bitmap, twice the size of the cursor image */
	desc.width = CURSOR_WIDTH * 2;
	desc.height = CURSOR_HEIGHT * 2;
	desc.bits_per_pixel = 0;
	desc.palette = NULL;
	combined = desc;
	status = gmgr_screen->surf.vtbl->SurfConvertBitmap(&gmgr_screen->surf,
		&combined.data, &combined.pitch,
		&desc);
	if (status != 0)
	{
		_wdprintf(L"allocate combined: %s\n", _wcserror(errno));
		goto error0;
	}

	/* Create under bitmap */
	desc.width = CURSOR_WIDTH;
	desc.height = CURSOR_HEIGHT;
	under = desc;
	status = gmgr_screen->surf.vtbl->SurfConvertBitmap(&gmgr_screen->surf,
		&under.data, &under.pitch,
		&desc);
	if (status != 0)
	{
		_wdprintf(L"allocate under: %s\n", _wcserror(errno));
		goto error1;
	}

	/* Create image bitmap, from bits */
	desc.bits_per_pixel = 1;
	desc.pitch = sizeof(bits) / CURSOR_HEIGHT;
	desc.palette = palette2;
	desc.data = bits;
	image = desc;

	status = gmgr_screen->surf.vtbl->SurfConvertBitmap(&gmgr_screen->surf,
		&image.data, &image.pitch,
		&desc);
	if (status != 0)
	{
		_wdprintf(L"allocate image: %s\n", _wcserror(errno));
		goto error2;
	}

	return true;

error2:
	free(under.data);
	under.data = NULL;
error1:
	free(combined.data);
	combined.data = NULL;
error0:
	return false;
}
