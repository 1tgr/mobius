/* $Id$ */
#ifndef __VGA_H
#define __VGA_H

/* I/O addresses for VGA registers */
#define VGA_AC_INDEX		0x3C0
#define VGA_AC_WRITE		0x3C0
#define VGA_AC_READ			0x3C1
#define VGA_MISC_WRITE		0x3C2
#define VGA_SEQ_INDEX		0x3C4
#define VGA_SEQ_DATA		0x3C5
#define VGA_PALETTE_MASK	0x3C6
#define VGA_DAC_READ_INDEX	0x3C7
#define VGA_DAC_WRITE_INDEX	0x3C8
#define VGA_DAC_DATA		0x3C9
#define VGA_MISC_READ		0x3CC
#define VGA_GC_INDEX		0x3CE
#define VGA_GC_DATA			0x3CF
#define VGA_CRTC_INDEX		0x3D4
#define VGA_CRTC_DATA		0x3D5
#define VGA_INSTAT_READ		0x3DA

/* number of registers in each VGA unit */
#define NUM_CRTC_REGS		25
#define NUM_AC_REGS			21
#define NUM_GC_REGS			9
#define NUM_SEQ_REGS		5
#define NUM_OTHER_REGS		1
#define NUM_REGS		\
	(NUM_CRTC_REGS + NUM_AC_REGS + NUM_GC_REGS + NUM_SEQ_REGS + NUM_OTHER_REGS)

/* VGA registers saving indexes */
#define CRT		0						/* CRT Controller Registers start */
#define ATT		(CRT+NUM_CRTC_REGS)		/* Attribute Controller Registers start */
#define GRA		(ATT+NUM_AC_REGS)		/* Graphics Controller Registers start */
#define SEQ		(GRA+NUM_GC_REGS)		/* Sequencer Registers */
#define MIS		(SEQ+NUM_SEQ_REGS)		/* General Registers */
#define EXT		(MIS+NUM_OTHER_REGS)	/* SVGA Extended Registers */

#define _CRTCBaseAdr		0x3c0

#endif
