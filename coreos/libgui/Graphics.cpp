// $Id$

#include <gui/Graphics.h>
#include <gui/Window.h>
#include <os/syscall.h>
#include <os/defs.h>

using namespace OS;

void Graphics::DrawText(const rect_t& rect, const wchar_t *str, ssize_t length)
{
	if (length == -1)
		length = wcslen(str);

	GfxDrawText(m_handle, &rect, str, length);
}


point_t Graphics::GetTextSize(const wchar_t *str, ssize_t length)
{
	point_t ret;

	if (length == -1)
		length = wcslen(str);

	GfxGetTextSize(m_handle, str, length, &ret);
	return ret;
}


Painter::Painter(Window *wnd) : Graphics(wnd->BeginPaint(&m_invalid_rect))
{
}


Painter::~Painter()
{
	Flush();
}


Bitmap::Bitmap(unsigned pixel_width, unsigned pixel_height) : 
	Graphics(GfxCreateBitmap(pixel_width, pixel_height, &m_desc))
{
	if (m_handle != 0)
	{
		m_data_area = HndOpen(m_desc.area_name);
		m_data = VmmMapSharedArea(m_data_area, 0, VM_MEM_READ | VM_MEM_WRITE | VM_MEM_ZERO | VM_MEM_USER);
	}
}


Bitmap::~Bitmap()
{
	HndClose(m_data_area);
	GfxClose(m_handle);
}
