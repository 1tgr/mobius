// ResFont.h: interface for the CResFont class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RESFONT_H__58AD83B0_C15E_43C3_9690_8501C38D16BC__INCLUDED_)
#define AFX_RESFONT_H__58AD83B0_C15E_43C3_9690_8501C38D16BC__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <gui/font.h>

#pragma pack(push, 1)

/* font file header (Adaptation Guide section 6.4) */
struct FontDirEntry {
    word        Version;              /* Always 17985 for the Nonce */
    dword       Size;                 /* Size of whole file */
    char        Copyright[60];
    word        Type;                 /* Raster Font if Type & 1 == 0 */
    word        Points;               /* Nominal Point size */
    word        VertRes;              /* Nominal Vertical resolution */
    word        HorizRes;             /* Nominal Horizontal resolution */
    word        Ascent;               /* Height of Ascent */
    word        IntLeading;           /* Internal (Microsoft) Leading */
    word        ExtLeading;           /* External (Microsoft) Leading */
    byte        Italic;               /* Italic font if set */
    byte        Underline;            /* Etc. */
    byte        StrikeOut;            /* Etc. */
    word        Weight;               /* Weight: 200 = regular */
    byte        CharSet;              /* ANSI=0. other=255 */
    word        PixWidth;             /* Fixed width. 0 ==> Variable */
    word        PixHeight;            /* Fixed Height */
    byte        Family;               /* Pitch and Family */
    word        AvgWidth;             /* Width of character 'X' */
    word        MaxWidth;             /* Maximum width */
    byte        FirstChar;            /* First character defined in font */
    byte        LastChar;             /* Last character defined in font */
    byte        DefaultChar;          /* Sub. for out of range chars. */
    byte        BreakChar;            /* word Break Character */
    word        Widthbytes;           /* No.bytes/row of Bitmap */
    dword       Device;               /* Pointer to Device Name string */
    dword       Face;                   /* Pointer to Face Name String */
    dword       BitsPointer;            /* Pointer to Bit Map */
    dword       BitsOffset;             /* Offset to Bit Map */
	byte		Reserved;
	dword		Flags;
	word		Aspace;
	word		Bspace;
	word		Cspace;
	dword		ColourPointer;
	byte		Reserved1[16];
};           /* Above pointers all rel. to start of file */

struct GlyphInfo20 {
     word GIwidth;
     word GIoffset;
};

#define SIZEOF_FONTDIRENTRY20	(sizeof(FontDirEntry) - 30)

#pragma pack(pop)

class CResFont : 
	public IUnknown, 
	public IFont  
{
protected:
	const FontDirEntry* m_header;
	const GlyphInfo20* m_chartable;

public:
	CResFont(dword inst, word id);
	virtual ~CResFont();

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN(CResFont);

	STDMETHOD(DrawText)(ISurface* pSurf, int x, int y, const wchar_t* str,
		pixel_t pixColour);
	STDMETHOD(GetTextExtent)(const wchar_t* str, point_t* size);
};

#endif // !defined(AFX_RESFONT_H__58AD83B0_C15E_43C3_9690_8501C38D16BC__INCLUDED_)
