// ClipSurface.h: interface for the CClipSurface class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CLIPSURFACE_H__CE89407E_9481_4AC3_BE6A_242345B87028__INCLUDED_)
#define AFX_CLIPSURFACE_H__CE89407E_9481_4AC3_BE6A_242345B87028__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <gui/surface.h>

class CClipSurface : 
	public IUnknown, 
	public ISurface  
{
protected:
	ISurface* m_surf;
	rectangle_t m_rect;

public:
	CClipSurface(ISurface* surf, const rectangle_t* rect);
	virtual ~CClipSurface();

	STDMETHOD(QueryInterface)(REFIID iid, void** ppvObject);
	IMPLEMENT_IUNKNOWN(CClipSurface);

	STDMETHOD(SetPalette)(int nIndex, int red, int green, int blue);
	STDMETHOD(Blt)(ISurface* pSrc, int x, int y, int nWidth,
		int nHeight, int nSrcX, int nSrcY, pixel_t pixTrans);
	STDMETHOD(Lock)(surface_t* pDesc);
	STDMETHOD(Unlock)();
	STDMETHOD(GetSurfaceDesc)(surface_t* pDesc);
	STDMETHOD_(pixel_t, ColourMatch)(colour_t clr);

	STDMETHOD(SetPixel)(int x, int y, pixel_t pix);
	STDMETHOD_(pixel_t, GetPixel)(int x, int y);
	STDMETHOD_(DrawMode, SetDrawMode)(DrawMode mode);
	STDMETHOD(FillRect)(const rectangle_t* rect, pixel_t pix);
	STDMETHOD(Rect3d)(const rectangle_t* rect, pixel_t pixTop,
		pixel_t pixBottom, int nWidth);
	STDMETHOD(Rect)(const rectangle_t* rect, pixel_t pix, int nWidth);
	STDMETHOD(GetClipRect)(rectangle_t* rect);
};

#endif // !defined(AFX_CLIPSURFACE_H__CE89407E_9481_4AC3_BE6A_242345B87028__INCLUDED_)
