#ifndef __GUI_FONT_H
#define __GUI_FONT_H

#include <gui/surface.h>

#undef INTERFACE
#define INTERFACE	IFont
DECLARE_INTERFACE(IFont)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD(DrawText)(THIS_ ISurface* pSurf, int x, int y, const wchar_t* str,
		pixel_t pixColour) PURE;
	STDMETHOD(GetTextExtent)(THIS_ const wchar_t* str, point_t* size) PURE;
};

// {816B7D59-910A-4187-AEF6-F30EFEFE4AEE}
DEFINE_GUID(IID_IFont, 
0x816b7d59, 0x910a, 0x4187, 0xae, 0xf6, 0xf3, 0xe, 0xfe, 0xfe, 0x4a, 0xee);

#endif
