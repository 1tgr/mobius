/* $Id: video.h,v 1.4 2002/03/05 14:23:24 pavlovskii Exp $ */

#ifndef __VGA_H
#define __VGA_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/driver.h>
#include <os/video.h>

/* I/O addresses for VGA registers */
#define VGA_AC_INDEX		0x3C0
#define VGA_AC_WRITE		0x3C0
#define VGA_AC_READ		0x3C1
#define VGA_MISC_WRITE		0x3C2
#define VGA_SEQ_INDEX		0x3C4
#define VGA_SEQ_DATA		0x3C5
#define VGA_DAC_READ_INDEX	0x3C7
#define VGA_DAC_WRITE_INDEX	0x3C8
#define VGA_DAC_DATA		0x3C9
#define VGA_MISC_READ		0x3CC
#define VGA_GC_INDEX		0x3CE
#define VGA_GC_DATA		0x3CF
#define VGA_CRTC_INDEX		0x3D4
#define VGA_CRTC_DATA		0x3D5
#define VGA_INSTAT_READ		0x3DA

/* number of registers in each VGA unit */
#define NUM_CRTC_REGS		25
#define NUM_AC_REGS		21
#define NUM_GC_REGS		9
#define NUM_SEQ_REGS		5
#define NUM_OTHER_REGS		1
#define NUM_REGS		\
    (NUM_CRTC_REGS + NUM_AC_REGS + NUM_GC_REGS + NUM_SEQ_REGS + NUM_OTHER_REGS)

#define _CRTCBaseAdr		0x3c0

typedef struct video_t video_t;
struct video_t
{
    void (*vidClose)(video_t *vid);
    int  (*vidEnumModes)(video_t *vid, unsigned index, videomode_t *mode);
    bool (*vidSetMode)(video_t *vid, videomode_t *mode);

    void (*vidPutPixel)(video_t *vid, int x, int y, colour_t c);
    colour_t (*vidGetPixel)(video_t *vid, int x, int y);
    void (*vidHLine)(video_t *vid, int x1, int x2, int y, colour_t c);
    void (*vidVLine)(video_t *vid, int x, int y1, int y2, colour_t c);
    void (*vidLine)(video_t *vid, int x1, int y1, int x2, int y2, colour_t d);
    void (*vidFillRect)(video_t *vid, int x1, int y1, int x2, int y2, colour_t c);
    void (*vidTextOut)(video_t *vid, int x, int y, vga_font_t *font, 
	    const wchar_t *str, size_t len, colour_t fg, colour_t bg);
    void (*vidStorePalette)(video_t *vid, const rgb_t *entries, unsigned first,
	    unsigned count);
};

#define VID_ENUM_CONTINUE	1
#define VID_ENUM_ERROR		0
#define VID_ENUM_STOP		-1

void vgaWriteRegs(const uint8_t *regs);
void vgaStorePalette(video_t *vid, const rgb_t *entries, unsigned first,
		     unsigned count);
size_t wcsto437(char *mbstr, const wchar_t *wcstr, size_t count);

extern semaphore_t sem_vga;

#ifdef __cplusplus
}
#endif

#endif