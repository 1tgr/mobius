#ifndef __GUI_SURFACE_H
#define __GUI_SURFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <os/com.h>

typedef dword pixel_t;
typedef dword colour_t;

#define RGB(r, g, b)    ((byte) (b) | ((byte) (g) << 8) | ((byte) (r) << 16))
#define GetRValue(rgb)  ((dword) (rgb) & 0xff0000)
#define GetGValue(rgb)  ((dword) (rgb) & 0x00ff00)
#define GetBValue(rgb)  ((dword) (rgb) & 0x0000ff)

typedef struct surface_t surface_t;
struct surface_t
{
	int nWidth, nHeight, nBpp, nPitch;
	void* pMemory;
};

typedef struct rectangle_t rectangle_t;
struct rectangle_t
{
	int left, top, right, bottom;

#ifdef __cplusplus

	rectangle_t() { }

	rectangle_t(int x1, int y1, int x2, int y2)
	{
		left = x1;
		top = y1;
		right = x2;
		bottom = y2;
	}
	
	operator rectangle_t*()
	{
		return this;
	}

	void OffsetRect(int dx, int dy)
	{
		left += dx;
		top += dy;
		right += dx;
		bottom += dy;
	}

	void InflateRect(int dx, int dy)
	{
		left -= dx;
		top -= dy;
		right += dx;
		bottom += dy;
	}

	void UnionRect(const rectangle_t* rect)
	{
		if (!rect->IsEmpty())
		{
			left = min(left, rect->left);
			top = min(top, rect->top);
			right = max(right, rect->right);
			bottom = max(bottom, rect->bottom);
		}
	}

	bool IsEmpty() const
	{
		return left == 0 &&
			top == 0 &&
			right == 0 &&
			bottom == 0;
	}

	void SetEmpty()
	{
		left = right = top = bottom = 0;
	}

	int Width() const
	{
		return right - left;
	}

	int Height() const
	{
		return bottom - top;
	}

#endif
};

typedef struct point_t point_t;
struct point_t
{
	int x, y;

#ifdef __cplusplus

	point_t() { }

	point_t(int x1, int y1)
	{
		x = x1;
		y = y1;
	}

#endif
};

typedef enum
{
	modeCopy,
	modeXor,
	modeNot
} DrawMode;

#undef INTERFACE
#define INTERFACE	ISurface
DECLARE_INTERFACE(ISurface)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD(SetPalette)(THIS_ int nIndex, int red, int green, int blue) PURE;
	STDMETHOD(Blt)(THIS_ ISurface* pSrc, int x, int y, int nWidth,
		int nHeight, int nSrcX, int nSrcY, pixel_t pixTrans) PURE;
	STDMETHOD(Lock)(THIS_ surface_t* pDesc) PURE;
	STDMETHOD(Unlock)(THIS) PURE;
	STDMETHOD(GetSurfaceDesc)(THIS_ surface_t* pDesc) PURE;
	STDMETHOD_(pixel_t, ColourMatch)(THIS_ colour_t clr) PURE;

	STDMETHOD(SetPixel)(THIS_ int x, int y, pixel_t pix) PURE;
	STDMETHOD_(pixel_t, GetPixel)(THIS_ int x, int y) PURE;
	STDMETHOD_(DrawMode, SetDrawMode)(THIS_ DrawMode mode) PURE;
	STDMETHOD(FillRect)(THIS_ const rectangle_t* rect, pixel_t pix) PURE;
	STDMETHOD(Rect3d)(THIS_ const rectangle_t* rect, pixel_t pixTop,
		pixel_t pixBottom, int nWidth) PURE;
	STDMETHOD(Rect)(THIS_ const rectangle_t* rect, pixel_t pix, int nWidth) PURE;
	STDMETHOD(GetClipRect)(THIS_ rectangle_t* rect) PURE;
};

/*
 * Some ISurface methods are currently not marshallable to the kernel:
 *	SetPalette
 *	Blt
 *	Rect3d
 */

//HRESULT ISurface_SetPalette(ISurface* ptr, int nIndex, int red, int green, int blue);
//HRESULT ISurface_Blt(ISurface* ptr, ISurface* pSrc, int x, int y, int nWidth,
	//int nHeight, int nSrcX, int nSrcY, pixel_t pixTrans);
HRESULT ISurface_Lock(ISurface* ptr, surface_t* pDesc);
HRESULT ISurface_Unlock(ISurface* ptr);
HRESULT ISurface_GetSurfaceDesc(ISurface* ptr, surface_t* pDesc);
pixel_t ISurface_ColourMatch(ISurface* ptr, colour_t clr);

HRESULT ISurface_SetPixel(ISurface* ptr, int x, int y, pixel_t pix);
pixel_t ISurface_GetPixel(ISurface* ptr, int x, int y);
DrawMode ISurface_SetDrawMode(ISurface* ptr, DrawMode mode);
HRESULT ISurface_FillRect(ISurface* ptr, const rectangle_t* rect, pixel_t pix);
HRESULT ISurface_Rect3d(ISurface* ptr, const rectangle_t* rect, pixel_t pixTop,
	pixel_t pixBottom, int nWidth);
HRESULT ISurface_Rect(ISurface* ptr, const rectangle_t* rect, pixel_t pix, int nWidth);
HRESULT ISurface_GetClipRect(ISurface* ptr, rectangle_t* rect);

// {816B7D5a-910A-4187-AEF6-F30EFEFE4AEE}
DEFINE_GUID(IID_ISurface, 
0x816b7d5a, 0x910a, 0x4187, 0xae, 0xf6, 0xf3, 0xe, 0xfe, 0xfe, 0x4a, 0xee);

#ifdef __cplusplus
}
#endif

#endif
