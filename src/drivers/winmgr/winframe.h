#ifndef __WINFRAME_H
#define __WINFRAME_H

#include "window.h"

enum WindowHit
{
	hitNone,
	hitClient,
	hitCaption,
	hitTopLeft,
	hitTop,
	hitTopRight,
	hitRight,
	hitBottomRight,
	hitBottom,
	hitBottomLeft,
	hitLeft,
	hitTransparent
};

class CWindowFrame : public CWindow
{
protected:
	//Rectangle m_rectDrag;
	//bool m_bDrag;
	//Point m_ptDrag;

	//WindowHit HitTest(int x, int y);

public: 
	CWindowFrame(const windowdef_t* def, IWindowServer* srv);

	//virtual bool PreDispatchMessage(Message* pMsg);
	//virtual void OnMessage(const Message* pMsg);
	virtual void OnPaint(ISurface* pSurf, const rectangle_t* rect);
	virtual void AdjustForFrame(rectangle_t* rect);
	virtual void RemoveChild(CWindow* child);
};

#endif