// DesktopWnd.cpp: implementation of the CDesktopWnd class.
//
//////////////////////////////////////////////////////////////////////

#include "DesktopWnd.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void CDesktopWnd::OnPaint(ISurface* surf, const rectangle_t* rect)
{
	surf->FillRect(rect, surf->ColourMatch(RGB(0, 128, 128)));
}