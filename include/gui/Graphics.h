#ifndef __GUI_GRAPHICS_H
#define __GUI_GRAPHICS_H

#include "winmgr.h"
#include "types.h"

namespace OS
{

class Window;

//! Wraps a graphics handle
class GUI_EXPORT Graphics
{
public:
	gfx_h m_handle;

	static void Flush()
	{
		GfxFlush();
	}

	Graphics(gfx_h hgfx)
	{
		m_handle = hgfx;
	}

	virtual ~Graphics()
	{
	}

	void FillRect(const rect_t& rect)
	{
		GfxFillRect(m_handle, &rect);
	}

	void Rectangle(const rect_t& rect)
	{
		GfxRectangle(m_handle, &rect);
	}

	void DrawText(const rect_t& rect, const wchar_t *str, ssize_t length = -1);

	void SetFillColour(colour_t clr)
	{
		GfxSetFillColour(m_handle, clr);
	}

	void SetPenColour(colour_t clr)
	{
		GfxSetPenColour(m_handle, clr);
	}

	void Line(int x1, int y1, int x2, int y2)
	{
		GfxLine(m_handle, x1, y1, x2, y2);
	}

	point_t GetTextSize(const wchar_t *str, ssize_t length = -1);

	void Blt(const rect_t& dest_rect, Graphics* src, const rect_t& src_rect)
	{
		GfxBlt(m_handle, &dest_rect, src->m_handle, &src_rect);
	}
};

//! Allows painting on a window
class GUI_EXPORT Painter : public Graphics
{
public:
	Rect m_invalid_rect;

	Painter(Window *wnd);
	~Painter();
};

//! Bitmap
class GUI_EXPORT Bitmap : public Graphics
{
public:
	bitmap_desc_t m_desc;
	handle_t m_data_area;
	void *m_data;

	Bitmap(unsigned pixel_width, unsigned pixel_height);
	~Bitmap();
};

};

#endif
