// DesktopWnd.h: interface for the CDesktopWnd class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DESKTOPWND_H__4D710E2D_4AB3_406A_8E63_9C4385858341__INCLUDED_)
#define AFX_DESKTOPWND_H__4D710E2D_4AB3_406A_8E63_9C4385858341__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "window.h"

class CDesktopWnd : public CWindow
{
public:
	CDesktopWnd(const windowdef_t* def, IWindowServer* srv) : CWindow(def, srv) { }

	virtual void OnPaint(ISurface* surf, const rectangle_t* rect);
};

#endif // !defined(AFX_DESKTOPWND_H__4D710E2D_4AB3_406A_8E63_9C4385858341__INCLUDED_)
