// Window.h: interface for the CWindow class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WINDOW_H__5E636D91_60B2_47BF_BA22_0380D4BAAFA8__INCLUDED_)
#define AFX_WINDOW_H__5E636D91_60B2_47BF_BA22_0380D4BAAFA8__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <gui/winserve.h>

class CWindow : 
	public IUnknown, 
	public IWindow,
	public rectangle_t
{
public:
	CWindow *m_prev, *m_next, *m_first, *m_last, *m_parent;
	IWindowServer* m_srv;
	ISurface* m_surf;
	IFont* m_font;
	wchar_t* m_title;
	rectangle_t m_invalid_rect;
	dword m_attribs[3];

	CWindow(const windowdef_t* def, IWindowServer* srv);
	virtual ~CWindow();

	virtual void OnPaint(ISurface* surf, const rectangle_t* rect);
	virtual void AdjustForFrame(rectangle_t* rect) { }
	void PostMessage(dword message, dword param);
	virtual void RemoveChild(CWindow* child);

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN(CWindow);

	STDMETHOD_(dword, GetAttrib)(dword index);
	STDMETHOD_(ISurface*, GetSurface)();
	STDMETHOD(InvalidateRect)(const rectangle_t* rect);
	STDMETHOD(UpdateWindow)();
	STDMETHOD(DefWndProc)(dword message, dword param);
	STDMETHOD_(IWindow*, GetFirstChild)();
	STDMETHOD(ClientToScreen)(rectangle_t* rect);
};

#endif // !defined(AFX_WINDOW_H__5E636D91_60B2_47BF_BA22_0380D4BAAFA8__INCLUDED_)
