// S3Graphics.h: interface for the CS3Graphics class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_S3GRAPHICS_H__17417CB7_9D36_41EF_8FF5_FAF1C201C207__INCLUDED_)
#define AFX_S3GRAPHICS_H__17417CB7_9D36_41EF_8FF5_FAF1C201C207__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "surface.h"

/* This is for the hardware (card)-adjusted mode timing. */
struct ModeTiming {
    int pixelClock;		/* Pixel clock in kHz. */
    int HDisplay;		/* Horizontal Timing. */
    int HSyncStart;
    int HSyncEnd;
    int HTotal;
    int VDisplay;		/* Vertical Timing. */
    int VSyncStart;
    int VSyncEnd;
    int VTotal;
    int flags;
/* The following field are optionally filled in according to card */
/* specific parameters. */
    int programmedClock;	/* Actual clock to be programmed. */
    int selectedClockNo;	/* Index number of fixed clock used. */
    int CrtcHDisplay;		/* Actual programmed horizontal CRTC timing. */
    int CrtcHSyncStart;
    int CrtcHSyncEnd;
    int CrtcHTotal;
    int CrtcVDisplay;		/* Actual programmed vertical CRTC timing. */
    int CrtcVSyncStart;
    int CrtcVSyncEnd;
    int CrtcVTotal;
};

/* Flags in ModeTiming. */
#define PHSYNC		0x1	/* Positive hsync polarity. */
#define NHSYNC		0x2	/* Negative hsync polarity. */
#define PVSYNC		0x4	/* Positive vsync polarity. */
#define NVSYNC		0x8	/* Negative vsync polarity. */
#define INTERLACED	0x10	/* Mode has interlaced timing. */
#define DOUBLESCAN	0x20	/* Mode uses VGA doublescan (see note). */
#define HADJUSTED	0x40	/* Horizontal CRTC timing adjusted. */
#define VADJUSTED	0x80	/* Vertical CRTC timing adjusted. */
#define USEPROGRCLOCK	0x100	/* A programmable clock is used. */

/* Mode info. */
struct ModeInfo {
/* Basic properties. */
    short width;		/* Width of the screen in pixels. */
    short height;		/* Height of the screen in pixels. */
    char bytesPerPixel;		/* Number of bytes per pixel. */
    char bitsPerPixel;		/* Number of bits per pixel. */
    char colorBits;		/* Number of significant bits in pixel. */
    char __padding1;
/* Truecolor pixel specification. */
    char redWeight;		/* Number of significant red bits. */
    char greenWeight;		/* Number of significant green bits. */
    char blueWeight;		/* Number of significant blue bits. */
    char __padding2;
    char redOffset;		/* Offset in bits of red value into pixel. */
    char blueOffset;		/* Offset of green value. */
    char greenOffset;		/* Offset of blue value. */
    char __padding3;
    unsigned redMask;		/* Pixel mask of read value. */
    unsigned blueMask;		/* Pixel mask of green value. */
    unsigned greenMask;		/* Pixel mask of blue value. */
/* Structural properties of the mode. */
    int lineWidth;		/* Offset in bytes between scanlines. */
    short realWidth;		/* Real on-screen resolution. */
    short realHeight;		/* Real on-screen resolution. */
    int flags;
};

/* Cards specifications. */
struct CardSpecs {
    int videoMemory;		/* Video memory in kilobytes. */
    int maxPixelClock4bpp;	/* Maximum pixel clocks in kHz for each depth. */
    int maxPixelClock8bpp;
    int maxPixelClock16bpp;
    int maxPixelClock24bpp;
    int maxPixelClock32bpp;
    int flags;			/* Flags (e.g. programmable clocks). */
    int nClocks;		/* Number of fixed clocks. */
    int *clocks;		/* Pointer to array of fixed clock values. */
    int maxHorizontalCrtc;
    /*
     * The following function maps from a pixel clock and depth to
     * the raw clock frequency required.
     */
    int (*mapClock) (int bpp, int pixelclock);
    /*
     * The following function maps from a requested clock value
     * to the closest clock that the programmable clock device
     * can produce.
     */
    int (*matchProgrammableClock) (int desiredclock);
    /*
     * The following function maps from a pixel clock, depth and
     * horizontal CRTC timing parameter to the horizontal timing
     * that has to be programmed.
     */
    int (*mapHorizontalCrtc) (int bpp, int pixelclock, int htiming);
};

struct MonitorModeTiming {
    int pixelClock;		/* Pixel clock in kHz. */
    int HDisplay;		/* Horizontal Timing. */
    int HSyncStart;
    int HSyncEnd;
    int HTotal;
    int VDisplay;		/* Vertical Timing. */
    int VSyncStart;
    int VSyncEnd;
    int VTotal;
    int flags;
    MonitorModeTiming *next;
};

class CS3Graphics : public Surface  
{
public:
	CS3Graphics();
	virtual ~CS3Graphics();

	IMPLEMENT_IUNKNOWN(CS3Graphics);

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

protected:
	int s3_flags, s3_chiptype, s3_memory;
	CardSpecs* cardspecs;
	int s3Mclk;

	void HwLock();
	void HwLockEnh();
	void HwUnlock();
	void HwUnlockEnh();
	int Init(int force, int par1, int par2);
	bool ModeAvailable(int mode);
	int AdjustLineWidth(int oldwidth);
	void InitializeMode(unsigned char *moderegs,
		ModeTiming * modetiming, ModeInfo * modeinfo);
	int SaveRegisters(unsigned char regs[]);
	void SetRegisters(const unsigned char regs[], int mode);
	int SetMode(int mode, int prv_mode);
};

#endif // !defined(AFX_S3GRAPHICS_H__17417CB7_9D36_41EF_8FF5_FAF1C201C207__INCLUDED_)
