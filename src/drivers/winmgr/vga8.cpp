#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/sys.h>
#include <string.h>
#include <stdio.h>
#include "surface.h"

class VgaGraphics : public Surface
{
private:
	bool fLocked;
	int m_palette_size;
	colour_t m_palette[256];

public:
	VgaGraphics(int nMode);
	virtual ~VgaGraphics();

	IMPLEMENT_IUNKNOWN(VgaGraphics);

	STDMETHOD(SetPalette)(int nIndex, int red, int green, int blue);
	STDMETHOD(Lock)(surface_t* pDesc);
	STDMETHOD(Unlock)();
	STDMETHOD(GetSurfaceDesc)(surface_t* pDesc);
	STDMETHOD_(pixel_t, ColourMatch)(colour_t clr);

	STDMETHOD(SetPixel)(int x, int y, pixel_t pix);
	STDMETHOD_(pixel_t, GetPixel)(int x, int y);
	STDMETHOD(Blt)(ISurface* pSrc, int x, int y, int nWidth,
		int nHeight, int nSrcX, int nSrcY, pixel_t pixTrans);
	STDMETHOD(FillRect)(const rectangle_t* rect, pixel_t pix);

	STDMETHOD(AttachProcess)();
};

ISurface* CreateGraphics(int nMode)
{
	return new VgaGraphics(nMode);
}

byte* vmem = (byte*) 0xa0000;

#define CRT_C   24      /* 24 CRT Controller Registers */
#define ATT_C   21      /* 21 Attribute Controller Registers */
#define GRA_C   9       /* 9  Graphics Controller Registers */
#define SEQ_C   5       /* 5  Sequencer Registers */
#define MIS_C   1       /* 1  Misc Output Register */

#define CRT_I   0x3D4       /* CRT Controller Index (mono: 0x3B4) */
#define ATT_IW  0x3C0       /* Attribute Controller Index & Data Write Register */
#define GRA_I   0x3CE       /* Graphics Controller Index */
#define SEQ_I   0x3C4       /* Sequencer Index */
#define PEL_IW  0x3C8       /* PEL Write Index */

#define CRT_D   0x3D5       /* CRT Controller Data Register (mono: 0x3B5) */
#define ATT_R   0x3C1       /* Attribute Controller Data Read Register */
#define GRA_D   0x3CF       /* Graphics Controller Data Register */
#define SEQ_D   0x3C5       /* Sequencer Data Register */
#define MIS_R   0x3CC       /* Misc Output Read Register */
#define MIS_W   0x3C2       /* Misc Output Write Register */
#define IS1_R   0x3DA       /* Input Status Register 1 (mono: 0x3BA) */
#define PEL_D   0x3C9       /* PEL Data Register */

#define PaletteMask              0x3C6  // bit mask register
#define PaletteRegisterRead      0x3C7  // read index
#define PaletteRegisterWrite     0x3C8  // write index
#define PaletteData              0x3C9  // send/receive data here

struct vgaregs_t
{
	byte crt[CRT_C];
	byte att[ATT_C];
	byte gra[GRA_C];
	byte seq[SEQ_C];
	byte mis[MIS_C];
};

static const vgaregs_t mode12 =
{
	{ 0x5F,0x4F,0x50,0x82,0x54,0x80,0x0B,0x3E,0x00,0x40,0x00,0x00,
	  0x00,0x00,0x00,0x59,0xEA,0x8C,0xDF,0x28,0x0F,0xE7,0x04,0xC3 },    // CRTC
	{ 0x00,0x08,0x3F,0x3F,0x18,0x18,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
	  0x3F,0x3F,0x3F,0x3F,0x01,0x00,0x0F,0x00,0x00 },           // ATTC
	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x05,0xFF },           // Graphics
	{ 0x03,0x01,0x0F,0x00,0x06 },                       // Sequencer
	{ 0xE3 }                                // Misc
};

// BIOS mode 13 - 320x200x8
static const vgaregs_t mode13 = {
  { 0x5F,0x4F,0x50,0x82,0x54,0x80,0xBF,0x1F,0x00,0x41,0x00,0x00,
	0x00,0x00,0x00,0x00,0x9C,0x8E,0x8F,0x28,0x40,0x96,0xB9,0xA3 },
  { 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,
	0x0C,0x0D,0x0E,0x0F,0x41,0x00,0x0F,0x00,0x00 },
  { 0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0F,0xFF },
  { 0x03,0x01,0x0F,0x00,0x0E },
  { 0x63 }
};

// VESA mode 100
static const vgaregs_t mode100 =
{
  { 0x63,0x4F,0x50,0x82,0x53,0xCC,0xC0,0x1F,0x00,0x40,0x00,0x00,
	0x00,0x00,0x00,0x00,0x9C,0x82,0x8F,0x50,0x00,0xE7,0x04,0xE3 },
  { 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,
	0x0C,0x0D,0x0E,0x0F,0x01,0x00,0x0F,0x00,0x00 },
  { 0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0F,0xFF },
  { 0x03,0x01,0x0F,0x00,0x0A },
  { 0x6B }
};

#include "palette.h"

// Saved text mode registers
static vgaregs_t text;

void WriteMode(const vgaregs_t* pMode)
{
	int i;

	for (i = 0; i < CRT_C; i++)
	{
		out(CRT_I, i);
		out(CRT_D, pMode->crt[i]);
	}

	in(IS1_R);

	for (i = 0; i < ATT_C; i++)
	{
		in(ATT_IW);
		out(ATT_IW, i);
		out(ATT_IW, pMode->att[i]);
	}

	out(ATT_IW, 0x20);

	for (i = 0; i < GRA_C; i++)
	{
		out(GRA_I, i);
		out(GRA_D, pMode->gra[i]);
	}

	for (i = 0; i < SEQ_C; i++)
	{
		out(SEQ_I, i);
		out(SEQ_D, pMode->seq[i]);
	}

	out(MIS_W, pMode->mis[0]);
}

void ReadMode(vgaregs_t* pMode)
{
	int i;

	for (i = 0; i < CRT_C; i++)
	{
		out(CRT_I, i);
		pMode->crt[i] = in(CRT_D);
	}

	for (i = 0; i < ATT_C; i++)
	{
		in(IS1_R);
		out(ATT_IW, i);
		pMode->att[i] = in(ATT_R);
	}

	for (i = 0; i < GRA_C; i++)
	{
		out(GRA_I, i);
		pMode->gra[i] = in(GRA_D);
	}

	for (i = 0; i < SEQ_C; i++)
	{
		out(SEQ_I, i);
		pMode->seq[i] = in(SEQ_D);
	}

	pMode->mis[0] = in(MIS_R);
}

//extern "C" void s3triofb_init_of(struct device_node *dp);
//extern "C" void trio_init();

#if 0
void thrWaitHandle(dword hnd)
{
	__asm
	{
		mov eax, 101h
		mov ebx, hnd
		int 30h
	}
}

void thrYield()
{
	__asm
	{
		mov eax, 2
		int 30h
	}
}

#pragma warning(disable:4035)
dword thrCreate86(const byte* code, size_t code_size)
{
	__asm
	{
		xor eax, eax
		mov ebx, code
		mov ecx, code_size
		int 30h
	}
}
#pragma warning(default:4035)
#endif

byte code86[] =
{
	0x8C, 0xC8, 		// MOV	AX,CS
	0x8E, 0xD8, 		// MOV	DS,AX
	0x8E, 0xC0, 		// MOV	ES,AX
	0x8E, 0xD0, 		// MOV	SS,AX
	0xB8, 0x13, 0x00,	// MOV	AX,13h
	0xCD, 0x10, 		// INT	10
//	0xEB, 0xFE,			// JMP  $
	0xB8, 0x06, 0x01,	// MOV	AX,0106h
	0xCD, 0x30,			// INT	30
};

/*byte code86[] =
{
	0xeb, 0xfe
};*/

//
// VgaGraphics::VgaGraphics
//
// Initializes the VGA card to the given mode (only 0x12 & 0x13 supported atm).
//
VgaGraphics::VgaGraphics(int nMode)
{
	int i;
	thread_t* thr;

	code86[9] = nMode;
	thr = thrCreate86(procCurrent(), code86, sizeof(code86), sysV86Fault);
	thrWaitHandle(thrCurrent(), thr);

	fWidth = 320;
	fHeight = 200;
	fBpp = 8;
	m_palette_size = 256;

	for (i = 0; i < m_palette_size; i++)
		SetPalette(i, palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]);

	m_refs = 0;
	fLocked = false;
	fDrawMode = modeCopy;

	//s3triofb_init_of(NULL);
	//trio_init();
}

VgaGraphics::~VgaGraphics()
{
	WriteMode(&text);
}

HRESULT VgaGraphics::AttachProcess()
{
	return S_OK;
}

//
// SetPalette
//
// Sets the palette index color to the colour (red, green, blue).
// red, green & blue are 8-bit integers.
//
HRESULT VgaGraphics::SetPalette(int color, int red, int green, int blue)
{
	out(PaletteMask, 0xff);
	out(PaletteRegisterWrite, color);
	out(PaletteData, red / 4);
	out(PaletteData, green / 4);
	out(PaletteData, blue / 4);
	m_palette[color] = RGB(red, green, blue);
	return S_OK;
}

HRESULT VgaGraphics::Lock(surface_t* pDesc)
{
	//HRESULT hr;

	if (fLocked)
		return E_FAIL;

	//if (FAILED(hr = VerifyArea(pDesc, sizeof(SurfaceDesc), PRIV_USER | PRIV_WR)))
		//return hr;

	GetSurfaceDesc(pDesc);
	pDesc->pMemory = vmem;
	return S_OK;
}

HRESULT VgaGraphics::Unlock()
{
	fLocked = false;
	return S_OK;
}

HRESULT VgaGraphics::GetSurfaceDesc(surface_t* pDesc)
{
	pDesc->nWidth = fWidth;
	pDesc->nHeight = fHeight;
	pDesc->nBpp = fBpp;
	pDesc->nPitch = fWidth;
	pDesc->pMemory = NULL;
	return S_OK;
}

pixel_t VgaGraphics::ColourMatch(colour_t clr)
{
	int i;

	for (i = 0; i < m_palette_size; i++)
		if (m_palette[i] == clr)
			return (byte) i;

	return 255;
}

HRESULT VgaGraphics::SetPixel(int x, int y, pixel_t pix)
{
	if (x < fWidth && y < fHeight)
	{
		switch (fDrawMode)
		{
		case modeCopy:
			vmem[x + y * fWidth] = (byte) pix;
			break;
		case modeXor:
			vmem[x + y * fWidth] ^= (byte) pix;
			break;
		case modeNot:
			vmem[x + y * fWidth] = ~vmem[x + y * fWidth];
			break;
		}
	}

	return S_OK;
}

pixel_t VgaGraphics::GetPixel(int x, int y)
{
	if (x < fWidth && y < fHeight)
		return vmem[x + y * fWidth];
	else
		return 0;
}

HRESULT VgaGraphics::Blt(ISurface* pSrc, int x, int y, int nWidth,
	int nHeight, int nSrcX, int nSrcY, pixel_t pixTrans)
{
	surface_t srcDesc;
	HRESULT hr;
	int i, j;
	pixel_t b;

	pSrc->GetSurfaceDesc(&srcDesc);
	if (srcDesc.nBpp == fBpp)
	{
		if (x < 0)
			x = 0;
		if (y < 0)
			y = 0;
		if (x + nWidth >= fWidth)
			nWidth = fWidth - x;
		if (y + nHeight >= fHeight)
			nHeight = fHeight - y;

		/*if (nSrcX < 0)
			nSrcX = 0;
		if (nSrcY < 0)
			nSrcY = 0;
		if (nSrcX + nWidth >= srcDesc.nWidth)
			nWidth = srcDesc.nWidth - nSrcX;
		if (nSrcY + nHeight >= srcDesc.nHeight)
			nHeight = srcDesc.nHeight - y;*/

		if (FAILED(hr = pSrc->Lock(&srcDesc)))
			return hr;

		if (pixTrans == (pixel_t) -1)
			for (i = 0; i < nHeight; i++)
				memcpy(vmem + x + (y + i) * fWidth,
					(byte*) srcDesc.pMemory + nSrcX + (nSrcY + i) * srcDesc.nWidth,
					nWidth);
		else
		{
			for (i = 0; i < nHeight; i++)
				for (j = 0; j < nWidth; j++)
				{
					b = ((byte*) srcDesc.pMemory)[nSrcX + j + (nSrcY + i) * srcDesc.nWidth];
					if (b != pixTrans)
						vmem[x + j + (y + i) * fWidth] = b;
				}
		}

		pSrc->Unlock();
		return S_OK;
	}
	else
		return Surface::Blt(pSrc, x, y, nWidth, nHeight, nSrcX, nSrcY, pixTrans);
}

HRESULT VgaGraphics::FillRect(const rectangle_t* rect, pixel_t pix)
{
	int x, y;
	surface_t desc;

	if (FAILED(Lock(&desc)))
		return E_FAIL;

	/*__asm
	{
		mov eax, rect
		mov ebx, [eax+4]
		mov ecx, [eax+8]
		mov edx, [eax+12]
		mov eax, [eax]
		mov esi, desc.pMemory
		cli
		hlt
	}*/

	for (y = rect->top; y < rect->bottom; y++)
		i386_lmemset((addr_t) desc.pMemory + y * desc.nPitch + rect->left, 
			pix, min(rect->Width(), fWidth - rect->left));

	/*for (y = rect->top; y < rect->bottom; y++)
		for (x = rect->left; y < rect->right; x++)
			((byte*) desc.pMemory) [x + y * fWidth] = pix;*/

	Unlock();
	return S_OK;
}
