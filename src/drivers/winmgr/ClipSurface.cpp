// ClipSurface.cpp: implementation of the CClipSurface class.
//
//////////////////////////////////////////////////////////////////////

#include "ClipSurface.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CClipSurface::CClipSurface(ISurface* surf, const rectangle_t* rect)
{
	m_surf = surf;
	m_rect = *rect;
	m_refs = 0;
}

CClipSurface::~CClipSurface()
{
	m_surf->Release();
}

HRESULT CClipSurface::QueryInterface(REFIID iid, void** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_ISurface))
	{
		AddRef();
		*ppvObject = (ISurface*) this;
		return S_OK;
	}
	else
		return E_FAIL;
}

HRESULT CClipSurface::SetPalette(int nIndex, int red, int green, int blue)
{
	return m_surf->SetPalette(nIndex, red, green, blue);
}

HRESULT CClipSurface::Blt(ISurface* pSrc, int x, int y, int nWidth,
	int nHeight, int nSrcX, int nSrcY, pixel_t pixTrans)
{
	return m_surf->Blt(pSrc, x + m_rect.left, y + m_rect.top, nWidth, nHeight, nSrcX, nSrcY, pixTrans);
}

HRESULT CClipSurface::Lock(surface_t* pDesc)
{
	return m_surf->Lock(pDesc);
}

HRESULT CClipSurface::Unlock()
{
	return m_surf->Unlock();
}

HRESULT CClipSurface::GetSurfaceDesc(surface_t* pDesc)
{
	return m_surf->GetSurfaceDesc(pDesc);
}

pixel_t CClipSurface::ColourMatch(colour_t clr)
{
	return m_surf->ColourMatch(clr);
}

HRESULT CClipSurface::SetPixel(int x, int y, pixel_t pix)
{
	return m_surf->SetPixel(x + m_rect.left, y + m_rect.top, pix);
}

pixel_t CClipSurface::GetPixel(int x, int y)
{
	return m_surf->GetPixel(x + m_rect.left, y + m_rect.top);
}

DrawMode CClipSurface::SetDrawMode(DrawMode mode)
{
	return m_surf->SetDrawMode(mode);
}

HRESULT CClipSurface::FillRect(const rectangle_t* rect, pixel_t pix)
{
	rectangle_t rc = *rect;
	rc.OffsetRect(m_rect.left, m_rect.top);
	return m_surf->FillRect(rc, pix);
}

HRESULT CClipSurface::Rect3d(const rectangle_t* rect, pixel_t pixTop,
	pixel_t pixBottom, int nWidth)
{
	rectangle_t rc = *rect;
	rc.OffsetRect(m_rect.left, m_rect.top);
	return m_surf->Rect3d(rc, pixTop, pixBottom, nWidth);
}

HRESULT CClipSurface::Rect(const rectangle_t* rect, pixel_t pix, int nWidth)
{
	rectangle_t rc = *rect;
	rc.OffsetRect(m_rect.left, m_rect.top);
	return m_surf->Rect(rc, pix, nWidth);
}

HRESULT CClipSurface::GetClipRect(rectangle_t* rect)
{
	*rect = m_rect;
	return S_OK;
}