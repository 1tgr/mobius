/* $Id: vga.c,v 1.4 2002/12/18 23:19:04 pavlovskii Exp $ */

#include <kernel/arch.h>
#include "include/video.h"

spinlock_t sem_vga;

#if 1
void vgaWriteRegs(const uint8_t *regs)
{
    unsigned i;
    volatile uint8_t a;

    SpinAcquire(&sem_vga);
    
    /* Send MISC regs */
    out(VGA_MISC_WRITE, *regs++);
    out(VGA_INSTAT_READ, *regs++);
    
    /* Send SEQ regs*/
    for (i = 0; i < 5; i++)
    {
        out(VGA_SEQ_INDEX, i);
        out(VGA_SEQ_DATA, *regs++);
    }
    
    /* Clear Protection bits */
    out16(VGA_CRTC_INDEX, 0xe11);
    
    /* Send CRTC regs */
    for (i = 0; i < 25; i++)
    {
        out(VGA_CRTC_INDEX, i);
        out(VGA_CRTC_DATA, *regs++);
    }

    /* Send GRAPHICS regs */
    for (i = 0; i < 9; i++)
    {
        out(VGA_GC_INDEX, i);
        out(VGA_GC_DATA, *regs++);
    }
    
    a = in(VGA_INSTAT_READ);
    
    /* Send ATTRCON regs */
    for (i = 0; i < 21; i++)
    {
        a = in(VGA_AC_INDEX);
        out(VGA_AC_INDEX, i);
        out(VGA_AC_WRITE, *regs++);
    }
    
    out(VGA_AC_WRITE, 0x20);
    SpinRelease(&sem_vga);
}
#else
void vgaWriteRegs(const uint8_t *regs)
{
}
#endif

void vgaStorePalette(video_t *vid, const rgb_t *entries, unsigned first, unsigned count)
{
    unsigned Index;

    /* start with palette entry 0 */
    SpinAcquire(&sem_vga);
    out(VGA_PALETTE_MASK, 0xff);
    out(VGA_DAC_WRITE_INDEX, first);
    for (Index = 0; Index < count; Index++)
    {
        out(VGA_DAC_DATA, entries[Index].red >> 2); /* red */   
        out(VGA_DAC_DATA, entries[Index].green >> 2); /* green */
        out(VGA_DAC_DATA, entries[Index].blue >> 2); /* blue */
    }

    SpinRelease(&sem_vga);
}

void *vgaSaveTextMemory(void)
{
    /*void *buf;
    buf = malloc(0x20000);
    memcpy(buf, PHYSICAL(0xa0000), 0x20000);
    return buf;*/
    return NULL;
}

void vgaRestoreTextMemory(void *buf)
{
    /*memcpy(PHYSICAL(0xa0000), buf, 0x20000);
    free(buf);*/
}
