#include <stdlib.h>
#include "surface.h"

HRESULT Surface::QueryInterface(REFIID iid, void ** ppvObject)
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

HRESULT Surface::Blt(ISurface* pSrc, int x, int y, int nWidth,
		int nHeight, int nSrcX, int nSrcY, pixel_t pixTrans)
{
	/*SurfaceDesc srcDesc;
	HRESULT hr;
	int line, nBytesPerLine;

	if (FAILED(hr = pSrc->Lock(&srcDesc)))
		return hr;

	if (srcDesc.nBpp != fBpp)
	{
		pSrc->Unlock();
		return E_NOTIMPLEMENTED;
	}

	nBytesPerLine = fBpp / 8;
	for (line = nSrcY; line < nSrcY + nHeight; line++)
		memcpy(pBuf + (nBytesPerLine * (fWidth * (line + y) + x)),
			(BYTE*) srcDesc.pMemory + srcDesc.nPitch * line + nSrcX * nBytesPerLine,
			nWidth * nBytesPerLine);

	pSrc->Unlock();
	return S_OK;*/

	int ax, ay;
	pixel_t pix;

	for (ax = 0; ax < nWidth; ax++)
		for (ay = 0; ay < nHeight; ay++)
			if (ax + x < fWidth &&
				ay + y < fHeight)
			{
				pix = pSrc->GetPixel(nSrcX + ax, nSrcY + ay);
				if (pix != pixTrans)
					SetPixel(ax + x, ay + y, pix);
			}

	return S_OK;
}

DrawMode Surface::SetDrawMode(DrawMode mode)
{
	DrawMode oldMode = fDrawMode;
	fDrawMode = mode;
	return oldMode;
}

HRESULT Surface::FillRect(const rectangle_t* rect, pixel_t pix)
{
	int x, y;

	for (y = max(0, rect->top); y < min(fHeight, rect->bottom); y++)
		for (x = max(0, rect->left); x < min(fWidth, rect->right); x++)
			SetPixel(x, y, pix);

	return S_OK;
}

HRESULT Surface::Rect3d(const rectangle_t* rect, pixel_t pixTop, pixel_t pixBottom,
	int nWidth)
{
	FillRect(rectangle_t(rect->left, rect->top, rect->right, rect->top + nWidth), pixTop);
	FillRect(rectangle_t(rect->left, rect->top + nWidth, rect->left + nWidth, rect->bottom), pixTop);
	FillRect(rectangle_t(rect->right - nWidth, rect->top + nWidth, rect->right, rect->bottom), pixBottom);
	FillRect(rectangle_t(rect->left + nWidth, rect->bottom - nWidth, rect->right - nWidth, rect->bottom), pixBottom);
	return S_OK;
}

HRESULT Surface::GetClipRect(rectangle_t* rect)
{
	rect->left = rect->top = 0;
	rect->right = fWidth;
	rect->bottom = fHeight;
	return S_OK;
}