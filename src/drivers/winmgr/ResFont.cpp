// ResFont.cpp: implementation of the CResFont class.
//
//////////////////////////////////////////////////////////////////////

#include "ResFont.h"
#include <os/os.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CResFont::CResFont(dword inst, word id)
{
	m_refs = 0;
	m_header = (const FontDirEntry*) resFind(inst, RT_FONT, id, 0);
	if (!m_header || m_header->Version != 0x200)
	{
		m_header = NULL;
		m_chartable = NULL;
	}
	else
		m_chartable = (const GlyphInfo20*) ((const byte*) m_header + SIZEOF_FONTDIRENTRY20);
}

CResFont::~CResFont()
{

}

HRESULT CResFont::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IFont))
	{
		AddRef();
		*ppvObject = (IFont*) this;
		return S_OK;
	}
	else
		return E_FAIL;
}
	
HRESULT CResFont::DrawText(ISurface* pSurf, int x, int y, const wchar_t* str,
	pixel_t pixColour)
{
	const GlyphInfo20 *gi;
	const byte* data;
	int cx, cy;
	pixel_t pix;
	surface_t desc;
	byte* buf;
	rectangle_t rect;

	if (!m_chartable || !m_header)
		return E_FAIL;
	
	if (SUCCEEDED(pSurf->Lock(&desc)))
	{
		pSurf->GetClipRect(&rect);
		x += rect.left;
		y += rect.top;
		buf = (byte*) desc.pMemory + y * desc.nPitch;

		for (; *str; str++)
		{
			gi = m_chartable + ((char) *str - m_header->FirstChar);
			data = (const byte*) m_header + gi->GIoffset;

			for (cy = 0; cy < m_header->PixHeight; cy++)
			{
				for (cx = 0; cx < gi->GIwidth; cx++)
				{
					pix = data[cy] & (0x80 >> cx);
					if (pix)
						buf[x + cx + cy * desc.nPitch] = pixColour;
						//pSurf->SetPixel(x + cx, y + cy, pixColour);
				}
			}

			x += gi->GIwidth;
		}

		pSurf->Unlock();
	}

	return S_OK;
}

HRESULT CResFont::GetTextExtent(const wchar_t* str, point_t* size)
{
	const GlyphInfo20 *gi;

	if (!m_chartable || !m_header)
	{
		size->x = size->y = 0;
		return E_FAIL;
	}

	size->x = 0;
	for (; *str; str++)
	{
		gi = m_chartable + ((char) *str - m_header->FirstChar);
		size->x += gi->GIwidth;
	}

	size->y = m_header->PixHeight;
	return S_OK;
}