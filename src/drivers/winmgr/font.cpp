#include <string.h>
#include <gui/font.h>

class VgaFont : public IFont
{
protected:
	const byte* pFontData;

public:
	VgaFont(const byte* buf);

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	IMPLEMENT_IUNKNOWN(VgaFont);

	STDMETHOD(DrawText)(ISurface* pSurf, int x, int y, const wchar_t* str,
		pixel_t pixColour);
	STDMETHOD(GetTextExtent)(const wchar_t* str, point_t* size);
};

IFont* CreateFont(const byte* pFontData)
{
	return new VgaFont(pFontData);
}

VgaFont::VgaFont(const byte* buf)
{
	pFontData = buf;
	m_refs = 0;
}

HRESULT VgaFont::QueryInterface(REFIID iid, void ** ppvObject)
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

HRESULT VgaFont::DrawText(ISurface* pSurf, int x, int y, const wchar_t* str, pixel_t pixColour)
{
	const byte *pCharData;
	const wchar_t* ch;
	byte pix, *buf;
	int cx, cy;
	surface_t desc;
	rectangle_t rect;

	if (SUCCEEDED(pSurf->Lock(&desc)))
	{
		pSurf->GetClipRect(&rect);
		x += rect.left;
		y += rect.top;
		buf = (byte*) desc.pMemory + y * desc.nPitch;

		for (ch = str; *ch; ch++)
		{
			pCharData = pFontData + (byte) *ch * 8;
			for (cy = 0; cy < 8; cy++)
			{
				if (pCharData[cy])
				{
					for (cx = 0; cx < 8; cx++)
					{
						pix = pCharData[cy] & (0x80 >> cx);
						if (pix)
							buf[x + cx + cy * desc.nPitch] = pixColour;
							//pSurf->SetPixel(x + cx, y + cy, pixColour);
					}
				}
			}

			x += 8;
		}

		pSurf->Unlock();
	}

	return S_OK;
}

HRESULT VgaFont::GetTextExtent(const wchar_t* str, point_t* size)
{
	size->x = wcslen(str) * 8;
	size->y = 8;
	return S_OK;
}
