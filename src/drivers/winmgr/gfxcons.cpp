#include "gfxcons.h"
#include <kernel/driver.h>
#include <os/os.h>
#include <gui/messages.h>

/*****************************************************************************
 * CGfxConsole                                                               *
 *****************************************************************************/

HRESULT ConsoleWndProc(IWindow* wnd, dword message, dword params)
{
	ISurface* surf;
	rectangle_t rect;

	switch (message)
	{
	case WM_PAINT:
		surf = wnd->GetSurface();
		surf->GetClipRect(&rect);
		surf->FillRect(&rect, surf->ColourMatch(0));
		surf->Release();
	}

	return wnd->DefWndProc(message, params);
}

CGfxConsole::CGfxConsole()
{
	IWindowServer* srv;
	windowdef_t def;

	m_width = 35;
	m_height = 22;

	def.size = sizeof(def);
	def.flags = WIN_TITLE | WIN_WIDTH | WIN_HEIGHT | WIN_WNDPROC;
	def.title = L"Console";
	def.width = 8 * m_width + 8;
	def.height = 8 * m_height + 8 + 4 + 12;
	def.wndproc = ConsoleWndProc;
		
	srv = OpenServer();
	if (srv)
	{
		m_font = srv->GetFont(0);
		m_wnd = srv->CreateWindow(&def);
		m_surf = m_wnd->GetSurface();
		srv->Release();
	}

	Clear();
}

CGfxConsole::~CGfxConsole()
{
	m_wnd->Release();
	m_surf->Release();
	m_font->Release();
}

void CGfxConsole::UpdateCursor()
{
	/*IMsgQueue* queue = (IMsgQueue*) thrGetTls();
	msg_t msg;
	HRESULT (*wndproc) (IWindow*, dword, dword);

	if (queue && SUCCEEDED(queue->PeekMessage(&msg, true)))
	{
		wndproc = (HRESULT (*) (IWindow*, dword, dword)) msg.wnd->GetAttrib(ATTR_WNDPROC);

		if (wndproc)
			wndproc(msg.wnd, msg.message, msg.params);
		else
			msg.wnd->DefWndProc(msg.message, msg.params);
	}*/
}

void CGfxConsole::Output(int x, int y, wchar_t c, word attrib)
{
	wchar_t str[2] = { c, '\0' };
	m_surf->FillRect(rectangle_t(x * 8, y * 8, x * 8 + 8, y * 8 + 8), attrib >> 12);
	m_font->DrawText(m_surf, x * 8, y * 8, str, (attrib >> 8) & 0xff);
}

void CGfxConsole::Clear()
{
	m_surf->FillRect(rectangle_t(0, 0, m_width * 8, m_height * 8), m_attrib >> 12);
	m_con_x = m_con_y = 0;
	UpdateCursor();
}

void CGfxConsole::Scroll(int dx, int dy)
{
	//i386_llmemcpy(0xa0000, 0xa0000 - dy * m_width * 64, 64 * m_width * (m_height + dy));
	//m_surf->FillRect(rectangle_t(0, (m_height - 1) * 8, m_width * 8, m_height * 8), m_attrib >> 12);
	Clear();
}