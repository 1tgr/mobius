// Window.cpp: implementation of the CWindow class.
//
//////////////////////////////////////////////////////////////////////

#include <os/os.h>
#include "Window.h"
#include "ClipSurface.h"
#include <string.h>
#include <malloc.h>
#include <gui/messages.h>
#include <kernel/proc.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//dword thrGetTls();

extern CWindow* desktop;

int new_window_x, new_window_y;

CWindow::CWindow(const windowdef_t* def, IWindowServer* srv)
{
	surface_t desc;

	m_refs = 0;
	m_srv = srv;
	m_surf = srv->GetScreen();
	m_font = srv->GetFont(1);
	m_first = m_last = m_prev = m_next = m_parent = NULL;

	m_surf->GetSurfaceDesc(&desc);

	if (def->flags & WIN_TITLE)
		m_title = wcsdup(def->title);
	else
		m_title = wcsdup(L"");

	if (def->flags & WIN_X)
		left = def->x;
	else
	{
		left = new_window_x;
		new_window_x += 20;
		if (new_window_x + 100 > desc.nWidth)
			new_window_x = 0;
	}

	if (def->flags & WIN_Y)
		top = def->y;
	else
	{
		top = new_window_y;
		new_window_y += 20;
		if (new_window_y + 100 > desc.nHeight)
			new_window_y = 0;
	}

	if (def->flags & WIN_WIDTH)
		right = left + def->width;
	else
		right = left + 100;

	if (def->flags & WIN_HEIGHT)
		bottom = top + def->height;
	else
		bottom = top + 100;

	if (def->flags & WIN_PARENT)
		m_parent = (CWindow*) def->parent;
	else
		m_parent = desktop;

	if (m_parent)
	{
		m_parent->AddRef();

		m_prev = m_parent->m_last;
		if (m_parent->m_last)
			m_parent->m_last->m_next = this;
		m_parent->m_last = this;
		if (m_parent->m_first == NULL)
			m_parent->m_first = this;
	}

	if (def->flags & WIN_WNDPROC)
		m_attribs[ATTR_WNDPROC] = (dword) def->wndproc;
	else
		m_attribs[ATTR_WNDPROC] = NULL;

	m_invalid_rect.SetEmpty();
	m_attribs[ATTR_PROCESS] = (dword) procCurrent();
	m_attribs[ATTR_MSGQUEUE] = thrGetInfo()->queue;
	InvalidateRect(this);
}

CWindow::~CWindow()
{
	if (m_prev)
		m_prev->m_next = m_next;
	if (m_next)
		m_next->m_prev = m_prev;

	if (m_parent)
	{
		m_parent->RemoveChild(this);
		m_parent->Release();
	}

	free(m_title);
	m_surf->Release();

	if (m_font)
		m_font->Release();

	m_srv->Release();
}

HRESULT CWindow::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IWindow))
	{
		AddRef();
		*ppvObject = (IWindow*) this;
		return S_OK;
	}
	else
		return E_FAIL;
}

void CWindow::OnPaint(ISurface* surf, const rectangle_t* rect)
{
}

void CWindow::PostMessage(dword message, dword param)
{
	IMsgQueue* queue = (IMsgQueue*) m_attribs[ATTR_MSGQUEUE];
	queue->PostMessage(this, message, param);
}

void CWindow::RemoveChild(CWindow* child)
{
	if (m_first == child)
		m_first = child->m_next;
	if (m_last == child)
		m_last = child->m_prev;

	InvalidateRect(child);
	UpdateWindow();
}

dword CWindow::GetAttrib(dword index)
{
	return m_attribs[index];
}

ISurface* CWindow::GetSurface()
{
	rectangle_t rect;
	m_surf->AddRef();
	rect = *this;
	if (m_parent)
		m_parent->ClientToScreen(&rect);
	return new CClipSurface(m_surf, rect);
}

HRESULT CWindow::InvalidateRect(const rectangle_t* rect)
{
	CWindow* child;

	m_invalid_rect.UnionRect(rect);

	for (child = m_first; child; child = child->m_next)
		child->InvalidateRect(rect);

	PostMessage(WM_PAINT, 0);
	return S_OK;
}

HRESULT CWindow::UpdateWindow()
{
	CWindow* child;
	ISurface* surf;

	if (!m_invalid_rect.IsEmpty())
	{
		surf = GetSurface();
		OnPaint(surf, m_invalid_rect);
		surf->Release();
		m_invalid_rect.SetEmpty();
	}

	for (child = m_first; child; child = child->m_next)
		child->UpdateWindow();

	return S_OK;
}

HRESULT CWindow::DefWndProc(dword message, dword param)
{
	switch (message)
	{
	case WM_PAINT:
		UpdateWindow();
		break;
	}

	return S_OK;
}

IWindow* CWindow::GetFirstChild()
{
	if (m_first)
	{
		m_first->AddRef();
		return m_first;
	}
	else
		return NULL;
}

HRESULT CWindow::ClientToScreen(rectangle_t* rect)
{
	if (m_parent)
		m_parent->ClientToScreen(rect);

	rect->OffsetRect(left, top);
	return S_OK;
}