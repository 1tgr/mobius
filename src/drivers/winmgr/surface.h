#ifndef __SURFACE_INT_H
#define __SURFACE_INT_H

#include <gui/surface.h>

class Surface : public ISurface
{
protected:
	int fWidth, fHeight, fBpp;
	DrawMode fDrawMode;

public:
	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);

	STDMETHOD_(DrawMode, SetDrawMode)(DrawMode mode);
	STDMETHOD(FillRect)(const rectangle_t* rect, pixel_t pix);
	STDMETHOD(Rect3d)(const rectangle_t* rect, pixel_t pixTop,
		pixel_t pixBottom, int nWidth);
	STDMETHOD(Rect)(const rectangle_t* rect, pixel_t pix, int nWidth)
		{ return Rect3d(rect, pix, pix, nWidth); }
	STDMETHOD(Blt)(ISurface* pSrc, int x, int y, int nWidth,
		int nHeight, int nSrcX, int nSrcY, pixel_t pixTrans);

	STDMETHOD(AttachProcess)()
		{ return S_OK; }
	STDMETHOD(GetClipRect)(rectangle_t* rect);
};

#endif