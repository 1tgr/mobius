// S3Graphics.cpp: implementation of the CS3Graphics class.
//
//////////////////////////////////////////////////////////////////////

#include "S3Graphics.h"
#include "driver.h"
#include <stdio.h>
#include <string.h>

MonitorModeTiming* current_timing;

#define CRT_C   24      /* 24 CRT Controller Registers */
#define ATT_C   21      /* 21 Attribute Controller Registers */
#define GRA_C   9       /* 9  Graphics Controller Registers */
#define SEQ_C   5       /* 5  Sequencer Registers */
#define MIS_C   1       /* 1  Misc Output Register */

#define VGA_TOTAL_REGS		60

#define CRT_IC  0x3D4       /* CRT Controller Index (mono: 0x3B4) */
#define ATT_IW  0x3C0       /* Attribute Controller Index & Data Write Register */
#define GRA_I   0x3CE       /* Graphics Controller Index */
#define SEQ_I   0x3C4       /* Sequencer Index */
#define PEL_IW  0x3C8       /* PEL Write Index */

/* VGA registers saving indexes */
#define CRT     0		/* CRT Controller Registers start */
#define ATT     (CRT+CRT_C)	/* Attribute Controller Registers start */
#define GRA     (ATT+ATT_C)	/* Graphics Controller Registers start */
#define SEQ     (GRA+GRA_C)	/* Sequencer Registers */
#define MIS     (SEQ+SEQ_C)	/* General Registers */
#define EXT     (MIS+MIS_C)	/* SVGA Extended Registers */

#define CRT_D   0x3D5       /* CRT Controller Data Register (mono: 0x3B5) */
#define ATT_R   0x3C1       /* Attribute Controller Data Read Register */
#define GRA_D   0x3CF       /* Graphics Controller Data Register */
#define SEQ_D   0x3C5       /* Sequencer Data Register */
#define MIS_R   0x3CC       /* Misc Output Read Register */
#define MIS_W   0x3C2       /* Misc Output Write Register */
#define IS1_R   0x3DA       /* Input Status Register 1 (mono: 0x3BA) */
#define PEL_D   0x3C9       /* PEL Data Register */

#define VGA_CRTC_OFFSET		0	/* 24 registers */
#define VGA_ATC_OFFSET		24	/* 21 registers */
#define VGA_GRAPHICS_OFFSET	45	/* 9 registers. */
#define VGA_SEQUENCER_OFFSET	54	/* 5 registers. */
#define VGA_MISCOUTPUT		59	/* (single register) */
#define VGA_TOTAL_REGS		60

#define PaletteMask              0x3C6  // bit mask register
#define PaletteRegisterRead      0x3C7  // read index
#define PaletteRegisterWrite     0x3C8  // write index
#define PaletteData              0x3C9  // send/receive data here

#define __svgalib_CRT_D			CRT_D
#define __svgalib_CRT_I			CRT_IC
#define __svgalib_IS1_R			IS1_R
#define __svgalib_driver_report	1

#define ADVFUNC_CNTL	0x4AE8	/* S3 */

int __svgalib_vga_inmisc(void)
{
	return in(MIS_R);
}

void __svgalib_vga_outmisc(int i)
{
	out(MIS_W,i);
}

int __svgalib_vga_incrtc(int i)
{
	out(__svgalib_CRT_I,i);
	return in(__svgalib_CRT_D);
}

void __svgalib_vga_outcrtc(int i, int d)
{
	out16(__svgalib_CRT_I,i|(d<<8));
}

int __svgalib_vga_inseq(int index)
{
    out(SEQ_I, index);
    return in(SEQ_D);
}

void __svgalib_vga_outseq(int index, int val)
{
    int v;
    v = ((int) val << 8) + index;
    out16(SEQ_I, v);
}

/*
* These are simple functions to write a value to a VGA Graphics
* Controller, CRTC Controller or Sequencer register.
* The idea is that drivers call these as a function call, making the
* code smaller and inserting small delays between I/O accesses when
* doing mode switches.
*/

void __svgalib_outGR(int index, unsigned char val)
{
    int v;
    v = ((int) val << 8) + index;
    out16(GRA_I, v);
}

void __svgalib_outbGR(int index, unsigned char val)
{
    out(GRA_I, index);
    out(GRA_D, val);
}

void __svgalib_outCR(int index, unsigned char val)
{
    int v;
    v = ((int) val << 8) + index;
    out16(CRT_IC, v);
}

void __svgalib_outbCR(int index, unsigned char val)
{
    out(CRT_IC, index);
    out(__svgalib_CRT_D, val);
}

void __svgalib_outSR(int index, unsigned char val)
{
    int v;
    v = ((int) val << 8) + index;
    out16(SEQ_I, v);
}

void __svgalib_outbSR(int index, unsigned char val)
{
    out(SEQ_I, index);
    out(SEQ_D, val);
}

unsigned char __svgalib_inGR(int index)
{
    out(GRA_I, index);
    return in(GRA_D);
}

unsigned char __svgalib_inCR(int index)
{
    out(CRT_IC, index);
    return in(CRT_D);
}

unsigned char __svgalib_inSR(int index)
{
    out(SEQ_I, index);
    return in(SEQ_D);
}

enum {
    S3_911, S3_924, S3_801, S3_805, S3_928, S3_864, S3_964, S3_TRIO32,
		S3_TRIO64, S3_866, S3_868, S3_968, S3_765
};

void CS3Graphics::HwLock()
{
    __svgalib_outCR(0x39, 0x00);		/* Lock system control regs. */
    __svgalib_outCR(0x38, 0x00);		/* Lock special regs. */
}

void CS3Graphics::HwLockEnh()
{
    if (s3_chiptype > S3_911)
		__svgalib_outCR(0x40, __svgalib_inCR(0x40) & ~0x01);    /* Lock enhanced command regs. */
    HwLock();
}

/*
* Unlock S3's registers.
* There are more locks, but this should suffice.
*/
void CS3Graphics::HwUnlock()
{
    __svgalib_outCR(0x38, 0x48);		/* Unlock special regs. */
    __svgalib_outCR(0x39, 0xA5);		/* Unlock system control regs. */
}

void CS3Graphics::HwUnlockEnh()
{
    Unlock();
    if (s3_chiptype > S3_911)
		__svgalib_outCR(0x40, __svgalib_inCR(0x40) | 0x01);     /* Unlock enhanced command regs. */
}

static const wchar_t *s3_chipname[] =
{L"911", L"924", L"801", L"805", L"928",
L"864", L"964", L"Trio32", L"Trio64", L"866", L"868", L"968", L"Trio64V+"};

/* flags used by this driver */
#define S3_LOCALBUS		0x01
#define S3_CLUT8_8		0x02
#define S3_OLD_STEPPING		0x04

/* Card flags. */
/* The card has programmable clocks (matchProgrammableClock is valid). */
#define CLOCK_PROGRAMMABLE		0x1
/* For interlaced modes, the vertical timing must be divided by two. */
#define INTERLACE_DIVIDE_VERT		0x2
/* For modes with vertical timing greater or equal to 1024, vertical */
/* timing must be divided by two. */
#define GREATER_1024_DIVIDE_VERT	0x4
/* The DAC doesn't support 64K colors (5-6-5) at 16bpp, just 5-5-5. */
#define NO_RGB16_565			0x8

int CS3Graphics::Init(int force, int par1, int par2)
{
    int id, rev, config;
	
    HwUnlock();
	
    s3_flags = 0;		/* initialize */
    id = __svgalib_inCR(0x30);		/* Get chip id. */
    rev = id & 0x0F;
    if (id >= 0xE0) {
		id |= __svgalib_inCR(0x2E) << 8;
		rev |= __svgalib_inCR(0x2F) << 4;
    }
    if (force) {
		s3_chiptype = par1;	/* we already know the type */
		s3_memory = par2;
		/* ARI: can we really trust the user's specification, or should we ignore
		it and probe ourselves ? */
		if (s3_chiptype == S3_801 || s3_chiptype == S3_805) {
			if ((rev & 0x0F) < 2)
				s3_flags |= S3_OLD_STEPPING;	/* can't handle 1152 width */
		} else if (s3_chiptype == S3_928) {
			if ((rev & 0x0F) < 4)	/* ARI: Stepping D or below */
				s3_flags |= S3_OLD_STEPPING;	/* can't handle 1152 width */
		}
    } else {
		s3_chiptype = -1;
		config = __svgalib_inCR(0x36);	/* get configuration info */
		switch (id & 0xf0) {
		case 0x80:
			if (rev == 1) {
				s3_chiptype = S3_911;
				break;
			}
			if (rev == 2) {
				s3_chiptype = S3_924;
				break;
			}
			break;
		case 0xa0:
			switch (config & 0x03) {
			case 0x00:
			case 0x01:
				/* EISA or VLB - 805 */
				s3_chiptype = S3_805;
				/* ARI: Test stepping: 0:B, 1:unknown, 2,3,4:C, 8:I, >=5:D */
				if ((rev & 0x0F) < 2)
					s3_flags |= S3_OLD_STEPPING;	/* can't handle 1152 width */
				break;
			case 0x03:
				/* ISA - 801 */
				s3_chiptype = S3_801;
				/* Stepping same as 805, just ISA */
				if ((rev & 0x0F) < 2)
					s3_flags |= S3_OLD_STEPPING;	/* can't handle 1152 width */
				break;
			}
			break;
			case 0x90:
				s3_chiptype = S3_928;
				if ((rev & 0x0F) < 4)	/* ARI: Stepping D or below */
					s3_flags |= S3_OLD_STEPPING;	/* can't handle 1152 width */
				break;
			case 0xB0:
				/* 928P */
				s3_chiptype = S3_928;
				break;
			case 0xC0:
				s3_chiptype = S3_864;
				break;
			case 0xD0:
				s3_chiptype = S3_964;
				break;
			case 0xE0:
				switch (id & 0xFFF0) {
				case 0x10E0:
					s3_chiptype = S3_TRIO32;
					break;
				case 0x3DE0: /* ViRGE/VX ID */
				case 0x31E0: /* ViRGE ID */
				case 0x01E0: /* S3Trio64V2/DX ... any others? */
				case 0x04E0:
				case 0x11E0:
					if (rev & 0x0400)
						s3_chiptype = S3_765;
					else
						s3_chiptype = S3_TRIO64;
					break;
				case 0x80E0:
					s3_chiptype = S3_866;
					break;
				case 0x90E0:
					s3_chiptype = S3_868;
					break;
				case 0xF0E0:	/* XXXX From data book; XF86 says 0xB0E0? */
					s3_chiptype = S3_968;
					break;
				}
		}
		if (s3_chiptype == -1) {
			wprintf(L"svgalib: S3: Unknown chip id %02x\n",
				id);
			return -1;
		}
		if (s3_chiptype <= S3_924) {
			if ((config & 0x20) != 0)
				s3_memory = 512;
			else
				s3_memory = 1024;
		} else {
			/* look at bits 5, 6 and 7 */
			switch ((config & 0xE0) >> 5) {
			case 0:
				s3_memory = 4096;
				break;
			case 2:
				s3_memory = 3072;
				break;
			case 3:
				s3_memory = 8192;
				break;
			case 4:
				s3_memory = 2048;
				break;
			case 5:
				s3_memory = 6144;
				break;
			case 6:
				s3_memory = 1024;
				break;
			case 7:
				s3_memory = 512;
				break;		/* Trio32 */
			}
		}
		
		if ((config & 0x03) < 3)	/* XXXX 928P is ignored */
			s3_flags |= S3_LOCALBUS;
    }
	
    if (__svgalib_driver_report) {
		wprintf(L"svgalib: Using S3 driver (%s, %dK).\n", s3_chipname[s3_chiptype],
			s3_memory);
		if (s3_flags & S3_OLD_STEPPING)
			wprintf(L"svgalib: Chip revision cannot handle modes with width 1152.\n");
		if (s3_chiptype > S3_TRIO64) {
			wprintf(L"svgalib: s3: chipsets newer than S3 Trio64 is not supported well yet.\n");
		}
    }
	/* begin: Initialize cardspecs. */
    /* If IOPERM is set, assume permissions have already been set by Olaf Titz' */
    /* ioperm(1). */
	
    /*if (getenv("IOPERM") == NULL) {
	if (0 > iopl(3))
	wprintf(L"svgalib: s3: cannot get I/O permissions for 8514.");
    }*/
#ifdef S3_LINEAR_SUPPORT
    if (s3_chiptype > S3_805) {
		int found_pciconfig;
		unsigned long pci_conf[64];
		
		found_pciconfig = __svgalib_pci_find_vendor_vga(0x5333, pci_conf, 0);
		if (!found_pciconfig)
			s3_linear_base = pci_conf[4] & 0xFF800000;
    }
    
    s3_cr59 = s3_linear_base >> 24;
    s3_cr5A = (s3_linear_base >> 16);
    if (! (s3_cr59 | s3_cr5A)) {
		s3_cr59 = __svgalib_inCR(0x59);
		s3_cr5A = __svgalib_inCR(0x5A);
		if (!s3_cr59) {
			s3_cr59 =  0xF3000000 >> 24;
			s3_cr5A = (0xF3000000 >> 16);
		}
		s3_linear_base = (s3_cr59 << 24) | (s3_cr5A << 16);
    }
    s3_linear_opt |= 0x10;
    switch (s3_memory) {
	case 512 :
	case 1024 : 
		s3_linear_opt |= 0x01;
		break;
	case 2048 :
		s3_linear_opt |= 0x02;
		break;
	case 3072 :
	case 4096 :
	case 6144 :
	case 8192 :
		s3_linear_opt |= 0x03;
		break;
	default :
		s3_linear_opt = 0x14;	/* like XFree */
    }
#endif
    
    cardspecs = (CardSpecs*) malloc(sizeof(CardSpecs));
    cardspecs->videoMemory = s3_memory;
    cardspecs->nClocks = 0;
    /*cardspecs->maxHorizontalCrtc = 2040; SL: kills 800x600x32k and above */
    cardspecs->maxHorizontalCrtc = 4088;
    cardspecs->flags = INTERLACE_DIVIDE_VERT;
	
    /* Process S3-specific config file options. */
    //__svgalib_read_options(s3_config_options, s3_process_option);
	
#ifdef INCLUDE_S3_TRIO64_DAC
    if ((s3_chiptype == S3_TRIO64 || s3_chiptype == S3_765) && dac_used == NULL)
		dac_used = &__svgalib_Trio64_methods;
#endif
	
    //if (dac_used == NULL)
	//dac_used = __svgalib_probeDacs(dacs_to_probe);
    //else
	//dac_used->initialize();
	
	
    //if (dac_used == NULL) {
	/* Not supported. */
	/*wprintf(L"svgalib: s3: Assuming normal VGA DAC.\n");
	#ifdef INCLUDE_NORMAL_DAC
	dac_used = &__svgalib_normal_dac_methods;
	dac_used->initialize();
	#else
	wprintf(L"svgalib: Alas, normal VGA DAC support is not compiled in, goodbye.\n");
	return 1;
	#endif
    }
    if (clk_used)
	clk_used->initialize(cardspecs, dac_used);*/
	
    //dac_used->qualifyCardSpecs(cardspecs, dac_speed);
	
    /* Initialize standard clocks for unknown DAC. */
    if ((!(cardspecs->flags & CLOCK_PROGRAMMABLE))
		&& cardspecs->nClocks == 0) {
		/*
		* Almost all cards have 25 and 28 MHz on VGA clocks 0 and 1,
		* so use these for an unknown DAC, yielding 640x480x256.
		*/
		cardspecs->nClocks = 2;
		cardspecs->clocks = (int*) malloc(sizeof(int) * 2);
		cardspecs->clocks[0] = 25175;
		cardspecs->clocks[1] = 28322;
    }
    /* Limit pixel clocks according to chip specifications. */
    if (s3_chiptype == S3_864 || s3_chiptype == S3_868) {
		/* Limit max clocks according to 95 MHz DCLK spec. */
		/* SL: might just be 95000 for 4/8bpp since no pixmux'ing */
		/*LIMIT(cardspecs->maxPixelClock4bpp, 95000 * 2);
		LIMIT(cardspecs->maxPixelClock8bpp, 95000 * 2);
		LIMIT(cardspecs->maxPixelClock16bpp, 95000);*/
		/* see explanation below */
		//LIMIT(cardspecs->maxPixelClock24bpp, 36000);
		/*
		* The official 32bpp limit is 47500, but we allow
		* 50 MHz for VESA 800x600 timing (actually the
		* S3-864 doesn't have the horizontal timing range
		* to run unmodified VESA 800x600 72 Hz timings).
		*/
		//LIMIT(cardspecs->maxPixelClock32bpp, 50000);
    }
#ifndef S3_16_COLORS
    //cardspecs->maxPixelClock4bpp = 0;	/* 16-color doesn't work. */
#endif
	
	/* end: Initialize cardspecs. */
	
    /*__svgalib_driverspecs = &__svgalib_s3_driverspecs;
	
	  __svgalib_banked_mem_base=0xa0000;
	  __svgalib_banked_mem_size=0x10000;
	  #ifdef S3_LINEAR_SUPPORT
	  __svgalib_linear_mem_base=s3_linear_base;
	  __svgalib_linear_mem_size=s3_memory*0x400;
#endif*/
	
    return 0;
}

#define S3_CR(n)	(EXT + (0x##n) - 0x30)

#define S3_CR30		S3_CR(30)
#define S3_CR31		S3_CR(31)
#define S3_CR32		S3_CR(32)
#define S3_CR33		S3_CR(33)
#define S3_CR34		S3_CR(34)
#define S3_CR35		S3_CR(35)
#define S3_CR3A		S3_CR(3A)
#define S3_CR3B		S3_CR(3B)
#define S3_CR3C		S3_CR(3C)
#define S3_CR40		S3_CR(40)
#define S3_CR42		S3_CR(42)
#define S3_CR43		S3_CR(43)
#define S3_CR44		S3_CR(44)
#define S3_CR50		S3_CR(50)	/* 801+ */
#define S3_CR51		S3_CR(51)
#define S3_CR53		S3_CR(53)
#define S3_CR54		S3_CR(54)
#define S3_CR55		S3_CR(55)
#define S3_CR58		S3_CR(58)
#define S3_CR59		S3_CR(59)
#define S3_CR5A		S3_CR(5A)
#define S3_CR5D		S3_CR(5D)
#define S3_CR5E		S3_CR(5E)
#define S3_CR60		S3_CR(60)
#define S3_CR61		S3_CR(61)
#define S3_CR62		S3_CR(62)
#define S3_CR67		S3_CR(67)
#define S3_CR6A		S3_CR(6A)
#define S3_CR6D		S3_CR(6D)

/* For debugging, these (non-)registers are read also (but never written). */

#define S3_CR36		S3_CR(36)
#define S3_CR37		S3_CR(37)
#define S3_CR38		S3_CR(38)
#define S3_CR39		S3_CR(39)
#define S3_CR3D		S3_CR(3D)
#define S3_CR3E		S3_CR(3E)
#define S3_CR3F		S3_CR(3F)
#define S3_CR45		S3_CR(45)
#define S3_CR46		S3_CR(46)
#define S3_CR47		S3_CR(47)
#define S3_CR48		S3_CR(48)
#define S3_CR49		S3_CR(49)
#define S3_CR4A		S3_CR(4A)
#define S3_CR4B		S3_CR(4B)
#define S3_CR4C		S3_CR(4C)
#define S3_CR4D		S3_CR(4D)
#define S3_CR4E		S3_CR(4E)
#define S3_CR4F		S3_CR(4F)
#define S3_CR52		S3_CR(52)
#define S3_CR56		S3_CR(56)
#define S3_CR57		S3_CR(57)
#define S3_CR5B		S3_CR(5B)
#define S3_CR5C		S3_CR(5C)
#define S3_CR5F		S3_CR(5F)
#define S3_CR63		S3_CR(63)
#define S3_CR64		S3_CR(64)
#define S3_CR65		S3_CR(65)
#define S3_CR66		S3_CR(66)
#define S3_CR6E		S3_CR(6E)
#define S3_CR6F		S3_CR(6F)

/* Trio extended SR registers */

#define S3_SR(n)	(S3_CR6F + 1 + (0x##n) - 0x08)

#define S3_SR08		S3_SR(08)
#define S3_SR09		S3_SR(09)
#define S3_SR0A		S3_SR(0A)
#define S3_SR0D		S3_SR(0D)
#define S3_SR10		S3_SR(10)
#define S3_SR11		S3_SR(11)
#define S3_SR12		S3_SR(12)
#define S3_SR13		S3_SR(13)
#define S3_SR15		S3_SR(15)
#define S3_SR18		S3_SR(18)
#define S3_SR1D		S3_SR(1D)

#define S3_8514_OFFSET	(S3_SR1D + 1)

#define S3_8514_COUNT	(1)	/* number of 2-byte words */

#define S3_DAC_OFFSET	(S3_8514_OFFSET + (S3_8514_COUNT * 2))

#define S3_TOTAL_REGS	(S3_DAC_OFFSET /*+ MAX_DAC_STATE*/)

/* 8514 regs */
#define S3_ADVFUNC_CNTL	0

static unsigned short s3_8514regs[S3_8514_COUNT] =
{
    /* default assuming text mode */
    0x0000U
};

/* 
* save S3 registers.  Lock registers receive special treatment
* so dumpreg will work under X.
*/
int CS3Graphics::SaveRegisters(unsigned char regs[])
{
    unsigned char b, bmax;
    unsigned char cr38, cr39, cr40;
	
    cr38 = __svgalib_inCR(0x38);
    __svgalib_outCR(0x38, 0x48);		/* unlock S3 VGA regs (CR30-CR3B) */
	
    cr39 = __svgalib_inCR(0x39);
    __svgalib_outCR(0x39, 0xA5);		/* unlock S3 system control (CR40-CR4F) */
    /* and extended regs (CR50-CR6D) */
	
    cr40 = __svgalib_inCR(0x40);		/* unlock enhanced regs */
    __svgalib_outCR(0x40, cr40 | 0x01);
	
    /* retrieve values from private copy */
    memcpy(regs + S3_8514_OFFSET, s3_8514regs, S3_8514_COUNT * 2);
	
    /* get S3 VGA/Ext registers */
    bmax = 0x4F;
    if (s3_chiptype >= S3_801)
		bmax = 0x66;
    if (s3_chiptype >= S3_864)
		bmax = 0x6D;
    for (b = 0x30; b <= bmax; b++)
		regs[EXT + b - 0x30] = __svgalib_inCR(b);
	
    /* get S3 ext. SR registers */
    /* if (s3_chiptype >= S3_864) { */
    if (s3_chiptype == S3_TRIO32 || s3_chiptype == S3_TRIO64
		|| s3_chiptype == S3_765) {/* SL: actually Trio32/64/V+ */
		regs[S3_SR08] = __svgalib_inSR(0x08);
		__svgalib_outSR(0x08, 0x06);	/* unlock extended seq regs */
		regs[S3_SR09] = __svgalib_inSR(0x09);
		regs[S3_SR0A] = __svgalib_inSR(0x0A);
		regs[S3_SR0D] = __svgalib_inSR(0x0D);
		regs[S3_SR10] = __svgalib_inSR(0x10);
		regs[S3_SR11] = __svgalib_inSR(0x11);
		regs[S3_SR12] = __svgalib_inSR(0x12);
		regs[S3_SR13] = __svgalib_inSR(0x13);
		regs[S3_SR15] = __svgalib_inSR(0x15);
		regs[S3_SR18] = __svgalib_inSR(0x18);
		__svgalib_outSR(0x08, regs[S3_SR08]);
    }
	
    //dac_used->saveState(regs + S3_DAC_OFFSET);
	
    /* leave the locks the way we found it */
    __svgalib_outCR(0x40, regs[EXT + 0x40 - 0x30] = cr40);
    __svgalib_outCR(0x39, regs[EXT + 0x39 - 0x30] = cr39);
    __svgalib_outCR(0x38, regs[EXT + 0x38 - 0x30] = cr38);
#if 0
#include "ramdac/IBMRGB52x.h"
	
    do {
		unsigned char m, n, df;
		
		printf("pix_fmt = 0x%02X, 8bpp = 0x%02X, 16bpp = 0x%02X, 24bpp = 0x%02X, 32bpp = 0x%02X,\n"
			"CR58 = 0x%02X, CR66 = 0x%02X, CR67 = 0x%02X, CR6D = 0x%02X\n",
			regs[S3_DAC_OFFSET + IBMRGB_pix_fmt],
			regs[S3_DAC_OFFSET + IBMRGB_8bpp],
			regs[S3_DAC_OFFSET + IBMRGB_16bpp],
			regs[S3_DAC_OFFSET + IBMRGB_24bpp],
			regs[S3_DAC_OFFSET + IBMRGB_32bpp],
			regs[S3_CR58],
			regs[S3_CR66],
			regs[S3_CR67],
			regs[S3_CR6D]);
		
		m = regs[S3_DAC_OFFSET + IBMRGB_m0 + 4];
		n = regs[S3_DAC_OFFSET + IBMRGB_n0 + 4];
		df = m >> 6;
		m &= ~0xC0;
		
		printf("m = 0x%02X %d, n = 0x%02X %d, df = 0x%02X %d, freq = %.3f\n",
			m, m, n, n, df, df, ((m + 65.0) / n) / (8 >> df) * 16.0);
    } while (0);
#endif
    return S3_DAC_OFFSET - VGA_TOTAL_REGS ;//+ dac_used->stateSize;
}

struct
{
	int xdim;
	int ydim;
	int bytesperpixel;
	int colors;
	int xbytes;
} __svgalib_infotable[] =
{
	{ 640, 480, 1, 256, 640 }
};

ModeInfo *
__svgalib_createModeInfoStructureForSvgalibMode(int mode)
{
    ModeInfo *modeinfo;
    /* Create the new ModeInfo structure. */
    modeinfo = (ModeInfo*) malloc(sizeof(ModeInfo));
    modeinfo->width = __svgalib_infotable[mode].xdim;
    modeinfo->height = __svgalib_infotable[mode].ydim;
    modeinfo->bytesPerPixel = __svgalib_infotable[mode].bytesperpixel;
    switch (__svgalib_infotable[mode].colors) {
    case 16:
		modeinfo->colorBits = 4;
		break;
    case 256:
		modeinfo->colorBits = 8;
		break;
    case 32768:
		modeinfo->colorBits = 15;
		modeinfo->blueOffset = 0;
		modeinfo->greenOffset = 5;
		modeinfo->redOffset = 10;
		modeinfo->blueWeight = 5;
		modeinfo->greenWeight = 5;
		modeinfo->redWeight = 5;
		break;
    case 65536:
		modeinfo->colorBits = 16;
		modeinfo->blueOffset = 0;
		modeinfo->greenOffset = 5;
		modeinfo->redOffset = 11;
		modeinfo->blueWeight = 5;
		modeinfo->greenWeight = 6;
		modeinfo->redWeight = 5;
		break;
    case 256 * 65536:
		modeinfo->colorBits = 24;
		modeinfo->blueOffset = 0;
		modeinfo->greenOffset = 8;
		modeinfo->redOffset = 16;
		modeinfo->blueWeight = 8;
		modeinfo->greenWeight = 8;
		modeinfo->redWeight = 8;
		break;
    }
    modeinfo->bitsPerPixel = modeinfo->bytesPerPixel * 8;
    if (__svgalib_infotable[mode].colors == 16)
		modeinfo->bitsPerPixel = 4;
    modeinfo->lineWidth = __svgalib_infotable[mode].xbytes;
    return modeinfo;
}

/*
* Clock allowance in 1/1000ths. 10 (1%) corresponds to a 250 kHz
* deviation at 25 MHz, 1 MHz at 100 MHz
*/
#define CLOCK_ALLOWANCE 10

#define PROGRAMMABLE_CLOCK_MAGIC_NUMBER 0x1234

static int findclock(int clock, CardSpecs * cardspecs)
{
    int i;
    /* Find a clock that is close enough. */
    for (i = 0; i < cardspecs->nClocks; i++) {
		int diff;
		diff = cardspecs->clocks[i] - clock;
		if (diff < 0)
			diff = -diff;
		if (diff * 1000 / clock < CLOCK_ALLOWANCE)
			return i;
    }
    /* Try programmable clocks if available. */
    if (cardspecs->flags & CLOCK_PROGRAMMABLE) {
		int diff;
		diff = cardspecs->matchProgrammableClock(clock) - clock;
		if (diff < 0)
			diff = -diff;
		if (diff * 1000 / clock < CLOCK_ALLOWANCE)
			return PROGRAMMABLE_CLOCK_MAGIC_NUMBER;
    }
    /* No close enough clock found. */
    return -1;
}

typedef struct {
/* refresh ranges in Hz */
    unsigned min;
    unsigned max;
} RefreshRange;

RefreshRange __svgalib_horizsync =
{31500U, 0U};			/* horz. refresh (Hz) min, max */
RefreshRange __svgalib_vertrefresh =
{50U, 70U};			/* vert. refresh (Hz) min, max */

/*
 * SYNC_ALLOWANCE is in percent
 * 1% corresponds to a 315 Hz deviation at 31.5 kHz, 1 Hz at 100 Hz
 */
#define SYNC_ALLOWANCE 1

#define INRANGE(x,y) \
    ((x) > __svgalib_##y.min * (1.0f - SYNC_ALLOWANCE / 100.0f) && \
     (x) < __svgalib_##y.max * (1.0f + SYNC_ALLOWANCE / 100.0f))

/*
 * Check monitor spec.
 */
static int timing_within_monitor_spec(MonitorModeTiming * mmtp)
{
    float hsf;			/* Horz. sync freq in Hz */
    float vsf;			/* Vert. sync freq in Hz */

    hsf = mmtp->pixelClock * 1000.0f / mmtp->HTotal;
    vsf = hsf / mmtp->VTotal;
    if ((mmtp->flags & INTERLACED))
	vsf *= 2.0f;
    if ((mmtp->flags & DOUBLESCAN))
	vsf /= 2.0f;
#if 1
    wprintf(L"hsf = %f (in:%d), vsf = %f (in:%d)\n",
	   hsf / 1000, (int) INRANGE(hsf, horizsync),
	   vsf, (int) INRANGE(vsf, vertrefresh));
#endif
    return INRANGE(hsf, horizsync) && INRANGE(vsf, vertrefresh);
}

static MonitorModeTiming *search_mode(MonitorModeTiming * timings,
				      int maxclock,
				      ModeInfo * modeinfo,
				      CardSpecs * cardspecs)
{
    int bestclock = 0;
    MonitorModeTiming *besttiming = NULL, *t;

    /*
     * bestclock is the highest pixel clock found for the resolution
     * in the mode timings, within the spec of the card and
     * monitor.
     * besttiming holds a pointer to timing with this clock.
     */

    /* Search the timings for the best matching mode. */
    for (t = timings; t; t = t->next)
	if (t->HDisplay == modeinfo->width
	    && t->VDisplay == modeinfo->height
	    && timing_within_monitor_spec(t)
	    && t->pixelClock <= maxclock
	    && t->pixelClock > bestclock
	    && cardspecs->mapHorizontalCrtc(modeinfo->bitsPerPixel,
					    t->pixelClock,
					    t->HTotal)
	    <= cardspecs->maxHorizontalCrtc
	/* Find the clock (possibly scaled by mapClock). */
	    && findclock(cardspecs->mapClock(modeinfo->bitsPerPixel,
					 t->pixelClock), cardspecs) != -1
	    ) {
	    bestclock = t->pixelClock;
	    besttiming = t;
	}
    return besttiming;
}

MonitorModeTiming __svgalib_standard_timings[] =
{
#define S __svgalib_standard_timings
/* 320x200 @ 70 Hz, 31.5 kHz hsync */
    {12588, 320, 336, 384, 400, 200, 204, 206, 225, DOUBLESCAN, S + 1},
/* 320x200 @ 83 Hz, 37.5 kHz hsync */
    {13333, 320, 336, 384, 400, 200, 204, 206, 225, DOUBLESCAN, S + 2},
/* 320x240 @ 60 Hz, 31.5 kHz hsync */
    {12588, 320, 336, 384, 400, 240, 245, 247, 263, DOUBLESCAN, S + 3},
/* 320x240 @ 72Hz, 38.5 kHz hsync */
    {15000, 320, 336, 384, 400, 240, 244, 246, 261, DOUBLESCAN, S + 4},
/* 320x400 @ 70 Hz, 31.5 kHz hsync */
    {12588, 320, 336, 384, 400, 400, 408, 412, 450, 0, S + 5},
/* 320x400 @ 83 Hz, 37.5 kHz hsync */
    {13333, 320, 336, 384, 400, 400, 408, 412, 450, 0, S + 6},
/* 320x480 @ 60 Hz, 31.5 kHz hsync */
    {12588, 320, 336, 384, 400, 480, 490, 494, 526, 0, S + 7},
/* 320x480 @ 72Hz, 38.5 kHz hsync */
    {15000, 320, 336, 384, 400, 480, 488, 492, 522, 0, S + 8},
/* 400x300 @ 56 Hz, 35.2 kHz hsync, 4:3 aspect ratio */
    {18000, 400, 416, 448, 512, 300, 301, 302, 312, DOUBLESCAN, S+9},
/* 400x300 @ 60 Hz, 37.8 kHz hsync */
    {20000, 400, 416, 480, 528, 300, 301, 303, 314, DOUBLESCAN, S+10},
/* 400x300 @ 72 Hz, 48.0 kHz hsync*/
    {25000, 400, 424, 488, 520, 300, 319, 322, 333, DOUBLESCAN, S+11},
/* 400x600 @ 56 Hz, 35.2 kHz hsync, 4:3 aspect ratio */
    {18000, 400, 416, 448, 512, 600, 602, 604, 624, 0, S+12},
/* 400x600 @ 60 Hz, 37.8 kHz hsync */
    {20000, 400, 416, 480, 528, 600, 602, 606, 628, 0, S+13},
/* 400x600 @ 72 Hz, 48.0 kHz hsync*/
    {25000, 400, 424, 488, 520, 600, 639, 644, 666, 0, S+14},
/* 512x384 @ 67Hz */
    {19600, 512, 522, 598, 646, 384, 418, 426, 454, 0, S+15 },
/* 512x384 @ 86Hz */
    {25175, 512, 522, 598, 646, 384, 418, 426, 454,0, S+16},
/* 512x480 @ 55Hz */
    {19600, 512, 522, 598, 646, 480, 500, 510, 550, 0, S+17},
/* 512x480 @ 71Hz */
    {25175, 512, 522, 598, 646, 480, 500, 510, 550,0, S+18},
/* 640x400 at 70 Hz, 31.5 kHz hsync */
    {25175, 640, 664, 760, 800, 400, 409, 411, 450, 0, S + 19},
/* 640x480 at 60 Hz, 31.5 kHz hsync */
    {25175, 640, 664, 760, 800, 480, 491, 493, 525, 0, S + 20},
/* 640x480 at 72 Hz, 36.5 kHz hsync */
    {31500, 640, 680, 720, 864, 480, 488, 491, 521, 0, S + 21},
/* 800x600 at 56 Hz, 35.15 kHz hsync */
    {36000, 800, 824, 896, 1024, 600, 601, 603, 625, 0, S + 22},
/* 800x600 at 60 Hz, 37.8 kHz hsync */
    {40000, 800, 840, 968, 1056, 600, 601, 605, 628, PHSYNC | PVSYNC, S + 23},
/* 800x600 at 72 Hz, 48.0 kHz hsync */
    {50000, 800, 856, 976, 1040, 600, 637, 643, 666, PHSYNC | PVSYNC, S + 24},
/* 960x720 @ 70Hz */
    {66000, 960, 984, 1112, 1248, 720, 723, 729, 756, NHSYNC | NVSYNC, S+25},
/* 960x720* interlaced, 35.5 kHz hsync */
    {40000, 960, 984, 1192, 1216, 720, 728, 784, 817, INTERLACED, S + 26},
/* 1024x768 at 87 Hz interlaced, 35.5 kHz hsync */
    {44900, 1024, 1048, 1208, 1264, 768, 776, 784, 817, INTERLACED, S + 27},
/* 1024x768 at 100 Hz, 40.9 kHz hsync */
    {55000, 1024, 1048, 1208, 1264, 768, 776, 784, 817, INTERLACED, S + 28},
/* 1024x768 at 60 Hz, 48.4 kHz hsync */
    {65000, 1024, 1032, 1176, 1344, 768, 771, 777, 806, NHSYNC | NVSYNC, S + 29},
/* 1024x768 at 70 Hz, 56.6 kHz hsync */
    {75000, 1024, 1048, 1184, 1328, 768, 771, 777, 806, NHSYNC | NVSYNC, S + 30},
/* 1152x864 at 59.3Hz */
    {85000, 1152, 1214, 1326, 1600, 864, 870, 885, 895, 0, S+31},
/* 1280x1024 at 87 Hz interlaced, 51 kHz hsync */
    {80000, 1280, 1296, 1512, 1568, 1024, 1025, 1037, 1165, INTERLACED, S + 32},
/* 1024x768 at 76 Hz, 62.5 kHz hsync */
    {85000, 1024, 1032, 1152, 1360, 768, 784, 787, 823, 0, S + 33},
/* 1280x1024 at 60 Hz, 64.3 kHz hsync */
    {110000, 1280, 1328, 1512, 1712, 1024, 1025, 1028, 1054, 0, S + 34},
/* 1280x1024 at 74 Hz, 78.9 kHz hsync */
    {135000, 1280, 1312, 1456, 1712, 1024, 1027, 1030, 1064, 0, S + 35},
/* 1600x1200 at 68Hz */
    {188500, 1600, 1792, 1856, 2208, 1200, 1202, 1205, 1256, 0, S + 36},
/* 1600x1200 at 75 Hz */
    {198000, 1600, 1616, 1776, 2112, 1200, 1201, 1204, 1250, 0, S + 37},
/* 720x540 at 56 Hz, 35.15 kHz hsync */
    {32400, 720, 744, 808, 920, 540, 541, 543, 563, 0, S + 38},
/* 720x540 at 60 Hz, 37.8 kHz hsync */
    {36000, 720, 760, 872, 952, 540, 541, 545, 565, 0, S + 39},
/* 720x540 at 72 Hz, 48.0 kHz hsync */
    {45000, 720, 768, 880, 936, 540, 552, 558, 599, 0, S + 40},
/* 1072x600 at 57 Hz interlaced, 35.5 kHz hsync */
    {44900, 1072, 1096, 1208, 1264, 600, 602, 604, 625, 0, S + 41},
/* 1072x600 at 65 Hz, 40.9 kHz hsync */
    {55000, 1072, 1096, 1208, 1264, 600, 602, 604, 625, 0, S + 42},
/* 1072x600 at 78 Hz, 48.4 kHz hsync */
    {65000, 1072, 1088, 1184, 1344, 600, 603, 607, 625, NHSYNC | NVSYNC, S + 43},
/* 1072x600 at 90 Hz, 56.6 kHz hsync */
    {75000, 1072, 1096, 1200, 1328, 768, 603, 607, 625, NHSYNC | NVSYNC, S + 44},
/* 1072x600 at 100 Hz, 62.5 kHz hsync */
    {85000, 1072, 1088, 1160, 1360, 768, 603, 607, 625, 0, NULL},
#undef S
};

int __svgalib_getmodetiming(ModeTiming * modetiming, ModeInfo * modeinfo,
							CardSpecs * cardspecs)
{
    int maxclock, desiredclock;
    MonitorModeTiming *besttiming=NULL;
	
    /*if(force_timing){
	if(timing_within_monitor_spec(force_timing) && 
	force_timing->HDisplay == modeinfo->width && 
	force_timing->VDisplay == modeinfo->height)
	{
	besttiming=force_timing;
	};
};*/
	
    /* Get the maximum pixel clock for the depth of the requested mode. */
    if (modeinfo->bitsPerPixel == 4)
		maxclock = cardspecs->maxPixelClock4bpp;
    else if (modeinfo->bitsPerPixel == 8)
		maxclock = cardspecs->maxPixelClock8bpp;
    else if (modeinfo->bitsPerPixel == 16)
	{
		if ((cardspecs->flags & NO_RGB16_565)
			&& modeinfo->greenWeight == 6)
			return 1;		/* No 5-6-5 RGB. */
		maxclock = cardspecs->maxPixelClock16bpp;
    }
	else if (modeinfo->bitsPerPixel == 24)
		maxclock = cardspecs->maxPixelClock24bpp;
    else if (modeinfo->bitsPerPixel == 32)
		maxclock = cardspecs->maxPixelClock32bpp;
    else
		maxclock = 0;
	
		/*
		* Check user defined timings first.
		* If there is no match within these, check the standard timings.
	*/
    //if(!besttiming)
	//besttiming = search_mode(user_timings, maxclock, modeinfo, cardspecs);
	besttiming = __svgalib_standard_timings + 20;
    if (!besttiming) {
	besttiming = search_mode(__svgalib_standard_timings, maxclock, modeinfo, cardspecs);
	if (!besttiming)
	{
		wprintf(L"besttiming == NULL\n");
		return 1;
	}
    }
    /*
	* Copy the selected timings into the result, which may
	* be adjusted for the chipset.
	*/
	
    modetiming->flags = besttiming->flags;
    modetiming->pixelClock = besttiming->pixelClock;	/* Formal clock. */
	
														/*
														* We know a close enough clock is available; the following is the
														* exact clock that fits the mode. This is probably different
														* from the best matching clock that will be programmed.
	*/
    desiredclock = cardspecs->mapClock(modeinfo->bitsPerPixel,
		besttiming->pixelClock);
	
    /* Fill in the best-matching clock that will be programmed. */
    modetiming->selectedClockNo = findclock(desiredclock, cardspecs);
    if (modetiming->selectedClockNo == PROGRAMMABLE_CLOCK_MAGIC_NUMBER) {
		modetiming->programmedClock =
			cardspecs->matchProgrammableClock(desiredclock);
		modetiming->flags |= USEPROGRCLOCK;
    } else
		modetiming->programmedClock = cardspecs->clocks[
		modetiming->selectedClockNo];
    modetiming->HDisplay = besttiming->HDisplay;
    modetiming->HSyncStart = besttiming->HSyncStart;
    modetiming->HSyncEnd = besttiming->HSyncEnd;
    modetiming->HTotal = besttiming->HTotal;
    if (cardspecs->mapHorizontalCrtc(modeinfo->bitsPerPixel,
		modetiming->programmedClock,
		besttiming->HTotal)
		!= besttiming->HTotal) {
		/* Horizontal CRTC timings are scaled in some way. */
		modetiming->CrtcHDisplay =
			cardspecs->mapHorizontalCrtc(modeinfo->bitsPerPixel,
			modetiming->programmedClock,
			besttiming->HDisplay);
		modetiming->CrtcHSyncStart =
			cardspecs->mapHorizontalCrtc(modeinfo->bitsPerPixel,
			modetiming->programmedClock,
			besttiming->HSyncStart);
		modetiming->CrtcHSyncEnd =
			cardspecs->mapHorizontalCrtc(modeinfo->bitsPerPixel,
			modetiming->programmedClock,
			besttiming->HSyncEnd);
		modetiming->CrtcHTotal =
			cardspecs->mapHorizontalCrtc(modeinfo->bitsPerPixel,
			modetiming->programmedClock,
			besttiming->HTotal);
		modetiming->flags |= HADJUSTED;
    } else {
		modetiming->CrtcHDisplay = besttiming->HDisplay;
		modetiming->CrtcHSyncStart = besttiming->HSyncStart;
		modetiming->CrtcHSyncEnd = besttiming->HSyncEnd;
		modetiming->CrtcHTotal = besttiming->HTotal;
    }
    modetiming->VDisplay = besttiming->VDisplay;
    modetiming->VSyncStart = besttiming->VSyncStart;
    modetiming->VSyncEnd = besttiming->VSyncEnd;
    modetiming->VTotal = besttiming->VTotal;
    if (modetiming->flags & DOUBLESCAN){
		modetiming->VDisplay <<= 1;
		modetiming->VSyncStart <<= 1;
		modetiming->VSyncEnd <<= 1;
		modetiming->VTotal <<= 1;
    }
    modetiming->CrtcVDisplay = modetiming->VDisplay;
    modetiming->CrtcVSyncStart = modetiming->VSyncStart;
    modetiming->CrtcVSyncEnd = modetiming->VSyncEnd;
    modetiming->CrtcVTotal = modetiming->VTotal;
    if (((modetiming->flags & INTERLACED)
		&& (cardspecs->flags & INTERLACE_DIVIDE_VERT))
		|| (modetiming->VTotal >= 1024
		&& (cardspecs->flags & GREATER_1024_DIVIDE_VERT))) {
		/*
		* Card requires vertical CRTC timing to be halved for
		* interlaced modes, or for all modes with vertical
		* timing >= 1024.
		*/
		modetiming->CrtcVDisplay /= 2;
		modetiming->CrtcVSyncStart /= 2;
		modetiming->CrtcVSyncEnd /= 2;
		modetiming->CrtcVTotal /= 2;
		modetiming->flags |= VADJUSTED;
    }
    current_timing=besttiming;
    return 0;			/* Succesful. */
}

/* Return non-zero if mode is available */

bool CS3Graphics::ModeAvailable(int mode)
{
    //struct info *info;
    ModeInfo *modeinfo;
    ModeTiming *modetiming;
	
    //if (mode < G640x480x256 || mode == G720x348x2)
	//return __svgalib_vga_driverspecs.modeavailable(mode);
	
    /* Enough memory? */
    //info = &__svgalib_infotable[mode];
    //if (s3_memory * 1024 < info->ydim * s3_adjlinewidth(info->xbytes))
	//return 0;
	
    modeinfo = __svgalib_createModeInfoStructureForSvgalibMode(mode);
	
    modetiming = (ModeTiming*) malloc(sizeof(ModeTiming));
    if (__svgalib_getmodetiming(modetiming, modeinfo, cardspecs))
	{
		wprintf(L"__svgalib_getmodetiming failed\n");
		free(modetiming);
		free(modeinfo);
		return 0;
    }
    free(modetiming);
    free(modeinfo);
	
    return true;
}

/* 
* Adjust the display width.  This is necessary for the graphics
* engine if acceleration is used.  However it will require more
* memory making some modes unavailable.
*/
int CS3Graphics::AdjustLineWidth(int oldwidth)
{
    if (s3_chiptype < S3_801)
		return 1024;
#ifdef S3_USE_GRAPHIC_ENGINE
    if (oldwidth <= 640)
		return 640;
    if (oldwidth <= 800)
		return 800;
    if (oldwidth <= 1024)
		return 1024;
    if (!(s3_flags & S3_OLD_STEPPING))
		if (oldwidth <= 1152)
			return 1152;
		if (oldwidth <= 1280)
			return 1280;
		if (oldwidth <= 1600 && s3_chiptype >= S3_864)
			return 1600;
		
		return 2048;
#else
		return oldwidth;
#endif
}

#define VGAREG_CR(i)		(i)
#define VGAREG_AR(i)		(i + VGA_ATC_OFFSET)
#define VGAREG_GR(i)		(i + VGA_GRAPHICS_OFFSET)
#define VGAREG_SR(i)		(i + VGA_SEQUENCER_OFFSET)

#define VGA_CR0			VGAREG_CR(0x00)
#define VGA_CR1			VGAREG_CR(0x01)
#define VGA_CR2			VGAREG_CR(0x02)
#define VGA_CR3			VGAREG_CR(0x03)
#define VGA_CR4			VGAREG_CR(0x04)
#define VGA_CR5			VGAREG_CR(0x05)
#define VGA_CR6			VGAREG_CR(0x06)
#define VGA_CR7			VGAREG_CR(0x07)
#define VGA_CR8			VGAREG_CR(0x08)
#define VGA_CR9			VGAREG_CR(0x09)
#define VGA_CRA			VGAREG_CR(0x0A)
#define VGA_CRB			VGAREG_CR(0x0B)
#define VGA_CRC			VGAREG_CR(0x0C)
#define VGA_CRD			VGAREG_CR(0x0D)
#define VGA_CRE			VGAREG_CR(0x0E)
#define VGA_CRF			VGAREG_CR(0x0F)
#define VGA_CR10		VGAREG_CR(0x10)
#define VGA_CR11		VGAREG_CR(0x11)
#define VGA_CR12		VGAREG_CR(0x12)
#define VGA_CR13		VGAREG_CR(0x13)
#define VGA_SCANLINEOFFSET	VGAREG_CR(0x13)
#define VGA_CR14		VGAREG_CR(0x14)
#define VGA_CR15		VGAREG_CR(0x15)
#define VGA_CR16		VGAREG_CR(0x16)
#define VGA_CR17		VGAREG_CR(0x17)
#define VGA_CR18		VGAREG_CR(0x18)

#define VGA_AR0			VGAREG_AR(0x00)
#define VGA_AR10		VGAREG_AR(0x10)
#define VGA_AR11		VGAREG_AR(0x11)
#define VGA_AR12		VGAREG_AR(0x12)
#define VGA_AR13		VGAREG_AR(0x13)
#define VGA_AR14		VGAREG_AR(0x14)

#define VGA_GR0			VGAREG_GR(0x00)
#define VGA_GR1			VGAREG_GR(0x01)
#define VGA_GR2			VGAREG_GR(0x02)
#define VGA_GR3			VGAREG_GR(0x03)
#define VGA_GR4			VGAREG_GR(0x04)
#define VGA_GR5			VGAREG_GR(0x05)
#define VGA_GR6			VGAREG_GR(0x06)
#define VGA_GR7			VGAREG_GR(0x07)
#define VGA_GR8			VGAREG_GR(0x08)

#define VGA_SR0			VGAREG_SR(0x00)
#define VGA_SR1			VGAREG_SR(0x01)
#define VGA_SR2			VGAREG_SR(0x02)
#define VGA_SR3			VGAREG_SR(0x03)
#define VGA_SR4			VGAREG_SR(0x04)

/*
* Setup VGA registers for SVGA mode timing. Adapted from XFree86,
* vga256/vga/vgaHW.c vgaHWInit().
*
* Note that VGA registers are set up in a way that is common for
* SVGA modes. This is not particularly useful for standard VGA
* modes, since VGA does not have a clean packed-pixel mode.
*/

void __svgalib_setup_VGA_registers(unsigned char *moderegs, ModeTiming * modetiming,
								   ModeInfo * modeinfo)
{
    int i;
	/* Sync Polarities */
    if ((modetiming->flags & (PHSYNC | NHSYNC)) &&
		(modetiming->flags & (PVSYNC | NVSYNC))) {
		/*
		* If both horizontal and vertical polarity are specified,
		* set them as specified.
		*/
		moderegs[VGA_MISCOUTPUT] = 0x23;
		if (modetiming->flags & NHSYNC)
			moderegs[VGA_MISCOUTPUT] |= 0x40;
		if (modetiming->flags & NVSYNC)
			moderegs[VGA_MISCOUTPUT] |= 0x80;
    } else {
	/*
	* Otherwise, calculate the polarities according to
	* monitor standards.
		*/
		if (modetiming->VDisplay < 400)
			moderegs[VGA_MISCOUTPUT] = 0xA3;
		else if (modetiming->VDisplay < 480)
			moderegs[VGA_MISCOUTPUT] = 0x63;
		else if (modetiming->VDisplay < 768)
			moderegs[VGA_MISCOUTPUT] = 0xE3;
		else
			moderegs[VGA_MISCOUTPUT] = 0x23;
    }
	
	/* Sequencer */
    moderegs[VGA_SR0] = 0x00;
    if (modeinfo->bitsPerPixel == 4)
		moderegs[VGA_SR0] = 0x02;
    moderegs[VGA_SR1] = 0x01;
    moderegs[VGA_SR2] = 0x0F;	/* Bitplanes. */
    moderegs[VGA_SR3] = 0x00;
    moderegs[VGA_SR4] = 0x0E;
    if (modeinfo->bitsPerPixel == 4)
		moderegs[VGA_SR4] = 0x06;
	
	/* CRTC Timing */
    moderegs[VGA_CR0] = (modetiming->CrtcHTotal / 8) - 5;
    moderegs[VGA_CR1] = (modetiming->CrtcHDisplay / 8) - 1;
    moderegs[VGA_CR2] = (modetiming->CrtcHSyncStart / 8) - 1;
    moderegs[VGA_CR3] = ((modetiming->CrtcHSyncEnd / 8) & 0x1F) | 0x80;
    moderegs[VGA_CR4] = (modetiming->CrtcHSyncStart / 8);
    moderegs[VGA_CR5] = (((modetiming->CrtcHSyncEnd / 8) & 0x20) << 2)
		| ((modetiming->CrtcHSyncEnd / 8) & 0x1F);
    moderegs[VGA_CR6] = (modetiming->CrtcVTotal - 2) & 0xFF;
    moderegs[VGA_CR7] = (((modetiming->CrtcVTotal - 2) & 0x100) >> 8)
		| (((modetiming->CrtcVDisplay - 1) & 0x100) >> 7)
		| ((modetiming->CrtcVSyncStart & 0x100) >> 6)
		| (((modetiming->CrtcVSyncStart) & 0x100) >> 5)
		| 0x10
		| (((modetiming->CrtcVTotal - 2) & 0x200) >> 4)
		| (((modetiming->CrtcVDisplay - 1) & 0x200) >> 3)
		| ((modetiming->CrtcVSyncStart & 0x200) >> 2);
    moderegs[VGA_CR8] = 0x00;
    moderegs[VGA_CR9] = ((modetiming->CrtcVSyncStart & 0x200) >> 4) | 0x40;
    if (modetiming->flags & DOUBLESCAN)
		moderegs[VGA_CR9] |= 0x80;
    moderegs[VGA_CRA] = 0x00;
    moderegs[VGA_CRB] = 0x00;
    moderegs[VGA_CRC] = 0x00;
    moderegs[VGA_CRD] = 0x00;
    moderegs[VGA_CRE] = 0x00;
    moderegs[VGA_CRF] = 0x00;
    moderegs[VGA_CR10] = modetiming->CrtcVSyncStart & 0xFF;
    moderegs[VGA_CR11] = (modetiming->CrtcVSyncEnd & 0x0F) | 0x20;
    moderegs[VGA_CR12] = (modetiming->CrtcVDisplay - 1) & 0xFF;
    moderegs[VGA_CR13] = modeinfo->lineWidth >> 4;	/* Just a guess. */
    moderegs[VGA_CR14] = 0x00;
    moderegs[VGA_CR15] = modetiming->CrtcVSyncStart & 0xFF;
    moderegs[VGA_CR16] = (modetiming->CrtcVSyncStart + 1) & 0xFF;
    moderegs[VGA_CR17] = 0xC3;
    if (modeinfo->bitsPerPixel == 4)
		moderegs[VGA_CR17] = 0xE3;
    moderegs[VGA_CR18] = 0xFF;
	
	/* Graphics Controller */
    moderegs[VGA_GR0] = 0x00;
    moderegs[VGA_GR1] = 0x00;
    moderegs[VGA_GR2] = 0x00;
    moderegs[VGA_GR3] = 0x00;
    moderegs[VGA_GR4] = 0x00;
    moderegs[VGA_GR5] = 0x40;
    if (modeinfo->bitsPerPixel == 4)
		moderegs[VGA_GR5] = 0x02;
    moderegs[VGA_GR6] = 0x05;
    moderegs[VGA_GR7] = 0x0F;
    moderegs[VGA_GR8] = 0xFF;
	
	/* Attribute Controller */
    for (i = 0; i < 16; i++)
		moderegs[VGA_AR0 + i] = i;
    moderegs[VGA_AR10] = 0x41;
    if (modeinfo->bitsPerPixel == 4)
		moderegs[VGA_AR10] = 0x01;	/* was 0x81 */
    /* Attribute register 0x11 is the overscan color. */
    moderegs[VGA_AR12] = 0x0F;
    moderegs[VGA_AR13] = 0x00;
    moderegs[VGA_AR14] = 0x00;
}

/* Color modes. */

#define CLUT8_6		0
#define CLUT8_8		1
#define RGB16_555	2
#define RGB16_565	3
#define RGB24_888_B	4	/* 3 bytes per pixel, blue byte first. */
#define RGB32_888_B	5	/* 4 bytes per pixel. */

/*
* This function converts a number of significant color bits to a matching
* DAC mode type as defined in the RAMDAC interface.
*/

int __svgalib_colorbits_to_colormode(int bpp, int colorbits)
{
    if (colorbits == 8)
		return CLUT8_6;
    if (colorbits == 15)
		return RGB16_555;
    if (colorbits == 16)
		return RGB16_565;
    if (colorbits == 24) {
		if (bpp == 24)
			return RGB24_888_B;
		else
			return RGB32_888_B;
    }
    return CLUT8_6;
}


/*
* Initialize register state for a mode.
*/

void CS3Graphics::InitializeMode(unsigned char *moderegs,
								 ModeTiming * modetiming, ModeInfo * modeinfo)
{
    /* Get current values. */
    SaveRegisters(moderegs);
	
    /* Set up the standard VGA registers for a generic SVGA. */
    __svgalib_setup_VGA_registers(moderegs, modetiming, modeinfo);
	
    /* Set up the extended register values, including modifications */
    /* of standard VGA registers. */
	
    moderegs[VGA_SR0] = 0x03;
    moderegs[VGA_CR13] = modeinfo->lineWidth >> 3;
    moderegs[VGA_CR17] = 0xE3;
	
    if (modeinfo->lineWidth / modeinfo->bytesPerPixel == 2048)
		moderegs[S3_CR31] = 0x8F;
    else
		moderegs[S3_CR31] = 0x8D;
#ifdef S3_LINEAR_MODE_BANKING_864
    if (s3_chiptype >= S3_864) {
		/* moderegs[S3_ENHANCEDMODE] |= 0x01; */
		/* Enable enhanced memory mode. */
		moderegs[S3_CR31] |= 0x04;
		/* Enable banking via CR6A in linear mode. */
		moderegs[S3_CR31] |= 0x01;
    }
#endif
    moderegs[S3_CR32] = 0;
    moderegs[S3_CR33] = 0x20;
    moderegs[S3_CR34] = 0x10;	/* 1024 */
    moderegs[S3_CR35] = 0;
    /* Call cebank() here when setting registers. */
    if (modeinfo->bitsPerPixel >= 8) {
		moderegs[S3_CR3A] = 0xB5;
		if (s3_chiptype == S3_928)
		/* ARI: Turn on CHAIN4 for 928, since __svgalib_setup_VGA_registers
							 initializes ModeX */
							 moderegs[VGA_CR14] = 0x60;
    } else {
		/* 16 color mode */
		moderegs[VGA_CR13] = modeinfo->lineWidth >> 1;
		moderegs[VGA_GR0] = 0x0F;
		moderegs[VGA_GR1] = 0x0F;
		moderegs[VGA_GR5] = 0x00;	/* write mode 0 */
		moderegs[VGA_AR11] = 0x00;
		moderegs[S3_CR3A] = 0x85;
    }
	
    moderegs[S3_CR3B] = (moderegs[VGA_CR0] + moderegs[VGA_CR4] + 1) / 2;
    moderegs[S3_CR3C] = moderegs[VGA_CR0] / 2;
    if (s3_chiptype == S3_911) {
		moderegs[S3_CR40] &= 0xF2;
		moderegs[S3_CR40] |= 0x09;
    } else if (s3_flags & S3_LOCALBUS) {
		moderegs[S3_CR40] &= 0xF2;
		/* Pegasus wants 0x01 for zero wait states. */
#ifdef S3_0_WAIT_805_864
		moderegs[S3_CR40] |= 0x09;	/* use fifo + 0 wait state */
#else
		moderegs[S3_CR40] |= 0x05;
#endif
    } else {
		moderegs[S3_CR40] &= 0xF6;
		moderegs[S3_CR40] |= 0x01;
    }
	
    if (modeinfo->bitsPerPixel >= 24) {
		/* 24/32 bit color */
		if (s3_chiptype == S3_864 || s3_chiptype == S3_964)
			moderegs[S3_CR43] = 0x08;
		else if (s3_chiptype == S3_928 /*&& dac_used->id == SIERRA_15025*/)
			moderegs[S3_CR43] = 0x01;	/* ELSA Winner 1000 */
    } else if (modeinfo->bitsPerPixel >= 15) {
		/* 15/16 bit color */
		if (s3_chiptype <= S3_864 || s3_chiptype >= S3_866) {	/* XXXX Trio? */
			moderegs[S3_CR43] = 0x08;
			//if (dac_used->id == IBMRGB52x)
			//moderegs[S3_CR43] = 0x10;
			//else
			if (s3_chiptype == S3_928 /*&& dac_used->id == SIERRA_15025*/)
				moderegs[S3_CR43] = 0x01;
			if (s3_chiptype <= S3_924 /*&& dac_used->id != NORMAL_DAC*/)
				moderegs[S3_CR43] = 0x01;
			
		} else
			/* XXXX some DAC might need this; XF86 source says... */
			moderegs[S3_CR43] = 0x09;
    } else {
		/* 4/8 bit color */
		moderegs[S3_CR43] = 0x00;
    }
	
    if (s3_chiptype >= S3_924 && s3_chiptype <= S3_928) {	/* different for 864+ */
		s3_8514regs[S3_ADVFUNC_CNTL] = 0x0002;
		if ((s3_chiptype == S3_928 && modeinfo->bitsPerPixel != 4) || !(s3_flags & S3_OLD_STEPPING))
			s3_8514regs[S3_ADVFUNC_CNTL] |= 0x0001;
		if (modeinfo->bitsPerPixel == 4)
			s3_8514regs[S3_ADVFUNC_CNTL] |= 0x0004;
#if 0
		/* 864 databook says it is for enhanced 4bpp */
		if (modeinfo->lineWidth > 640)
			s3_8514regs[S3_ADVFUNC_CNTL] |= 0x0004;
#endif
    } else if (s3_chiptype == S3_968) {
		s3_8514regs[S3_ADVFUNC_CNTL] = 0x0002;
		if (modeinfo->bitsPerPixel == 4)
			s3_8514regs[S3_ADVFUNC_CNTL] |= 0x0004;
#ifdef PIXEL_MULTIPLEXING
		else
			s3_8514regs[S3_ADVFUNC_CNTL] |= 0x0001;
#endif
    } else if (modeinfo->lineWidth / modeinfo->bytesPerPixel == 1024)
		s3_8514regs[S3_ADVFUNC_CNTL] = 0x0007;
    else
		s3_8514regs[S3_ADVFUNC_CNTL] = 0x0003;
	
    moderegs[S3_CR44] = 0;
    /* Skip CR45, 'hi/truecolor cursor color enable'. */
	
    if (s3_chiptype >= S3_801) {
		int m, n;		/* for FIFO balancing */
		
		/* XXXX Not all chips support all widths. */
		moderegs[S3_CR50] &= ~0xF1;
		switch (modeinfo->bitsPerPixel) {
		case 16:
			moderegs[S3_CR50] |= 0x10;
			break;
		case 24:		/* XXXX 868/968 only */
			if (s3_chiptype >= S3_868)
				moderegs[S3_CR50] |= 0x20;
			break;
		case 32:
			moderegs[S3_CR50] |= 0x30;
			break;
		}
		
		switch (modeinfo->lineWidth / modeinfo->bytesPerPixel) {
		case 640:
			moderegs[S3_CR50] |= 0x40;
			break;
		case 800:
			moderegs[S3_CR50] |= 0x80;
			break;
		case 1152:
			if (!(s3_flags & S3_OLD_STEPPING)) {
				moderegs[S3_CR50] |= 0x01;
				break;
			}			/* else fall through */
		case 1280:
			moderegs[S3_CR50] |= 0xC0;
			break;
		case 1600:
			moderegs[S3_CR50] |= 0x81;
			break;
			/* 1024/2048 no change. */
		}
		
		moderegs[S3_CR51] &= 0xC0;
		moderegs[S3_CR51] |= (modeinfo->lineWidth >> 7) & 0x30;
		
		/* moderegs[S3_CR53] |= 0x10; *//* Enable MMIO. */
		/* moderegs[S3_CR53] |= 0x20; *//* DRAM interleaving for S3_805i with 2MB */
		
		n = 0xFF;
		if (s3_chiptype >= S3_864 ||
			s3_chiptype == S3_801 || s3_chiptype == S3_805) {
			/* 
			* CRT FIFO balancing for DRAM cards and 964/968
			* in VGA mode.
			*/
			int clock, mclk;
			if (modeinfo->bitsPerPixel < 8) {
				clock = modetiming->pixelClock;
			} else {
				clock = modetiming->pixelClock *
					modeinfo->bytesPerPixel;
			}
			if (s3_memory < 2048 || s3_chiptype == S3_TRIO32)
				clock *= 2;
			if (s3Mclk > 0)
				mclk = s3Mclk;
			else if (s3_chiptype == S3_801 || s3_chiptype == S3_805)
				mclk = 50000;	/* Assumption. */
			else
				mclk = 60000;	/* Assumption. */
			m = (int) ((mclk / 1000.0 * .72 + 16.867) * 89.736 / (clock / 1000.0 + 39) - 21.1543);
			if (s3_memory < 2048 || s3_chiptype == S3_TRIO32)
				m /= 2;
			if (m > 31)
				m = 31;
			else if (m < 0) {
				m = 0;
				n = 16;
			}
		} else if (s3_memory == 512 || modetiming->HDisplay > 1200)
			m = 0;
		else if (s3_memory == 1024)
			m = 2;
		else
			m = 20;
		
		moderegs[S3_CR54] = m << 3;
		moderegs[S3_CR60] = n;
		
		moderegs[S3_CR55] &= 0x08;
		moderegs[S3_CR55] |= 0x40;
		
#ifdef S3_LINEAR_MODE_BANKING_864
		if (s3_chiptype >= S3_864) {
			if (modeinfo->bitsPerPixel >= 8) {
				/* Enable linear addressing. */
				moderegs[S3_CR58] |= 0x10;
				/* Set window size to 64K. */
				moderegs[S3_CR58] &= ~0x03;
				/* Assume CR59/5A are correctly set up for 0xA0000. */
				/* Set CR6A linear bank to zero. */
				moderegs[S3_CR6A] &= ~0x3F;
				/* use alternate __svgalib_setpage() function */
				__svgalib_s3_driverspecs.__svgalib_setpage = s3_setpage864;
			} else {
				/* doesn't work for 4bpp. */
				__svgalib_s3_driverspecs.__svgalib_setpage = s3_setpage;
			}
		}
#endif
#ifdef S3_LINEAR_SUPPORT
		moderegs[S3_CR59] = s3_cr59;
		moderegs[S3_CR5A] = s3_cr5A;
#endif
		
		/* Extended CRTC timing. */
		moderegs[S3_CR5E] =
			(((modetiming->CrtcVTotal - 2) & 0x400) >> 10) |
			(((modetiming->CrtcVDisplay - 1) & 0x400) >> 9) |
			(((modetiming->CrtcVSyncStart) & 0x400) >> 8) |
			(((modetiming->CrtcVSyncStart) & 0x400) >> 6) | 0x40;
		
		{
			int i, j;
			i = ((((modetiming->CrtcHTotal >> 3) - 5) & 0x100) >> 8) |
				((((modetiming->CrtcHDisplay >> 3) - 1) & 0x100) >> 7) |
				((((modetiming->CrtcHSyncStart >> 3) - 1) & 0x100) >> 6) |
				((modetiming->CrtcHSyncStart & 0x800) >> 7);
			if ((modetiming->CrtcHSyncEnd >> 3) - (modetiming->CrtcHSyncStart >> 3) > 64)
				i |= 0x08;
			if ((modetiming->CrtcHSyncEnd >> 3) - (modetiming->CrtcHSyncStart >> 3) > 32)
				i |= 0x20;
			j = ((moderegs[VGA_CR0] + ((i & 0x01) << 8) +
				moderegs[VGA_CR4] + ((i & 0x10) << 4) + 1) / 2);
			if (j - (moderegs[VGA_CR4] + ((i & 0x10) << 4)) < 4) {
				if (moderegs[VGA_CR4] + ((i & 0x10) << 4) + 4 <= moderegs[VGA_CR0] + ((i & 0x01) << 8))
					j = moderegs[VGA_CR4] + ((i & 0x10) << 4) + 4;
				else
					j = moderegs[VGA_CR0] + ((i & 0x01) << 8) + 1;
			}
            
			moderegs[S3_CR3B] = j & 0xFF;
			i |= (j & 0x100) >> 2;
			/* Interlace mode frame offset. */
			moderegs[S3_CR3C] = (moderegs[VGA_CR0] + ((i & 0x01) << 8)) / 2;
			moderegs[S3_CR5D] = (moderegs[S3_CR5D] & 0x80) | i;
		}
		
		{
			int i;
			
			if (modeinfo->bitsPerPixel < 8)
				i = modetiming->HDisplay / 4 + 1;
			else
				i = modetiming->HDisplay *
				modeinfo->bytesPerPixel / 4 + 1;
			
			moderegs[S3_CR61] = (i >> 8) | 0x80;
			moderegs[S3_CR62] = i & 0xFF;
		}
    }				/* 801+ */
    if (modetiming->flags & INTERLACED)
		moderegs[S3_CR42] |= 0x20;
	
		/*
		* Clock select works as follows:
		* Clocks 0 and 1 (VGA 25 and 28 MHz) can be selected via the
		* two VGA MiscOutput clock select bits.
		* If 0x3 is written to these bits, the selected clock index
		* is taken from the S3 clock select register at CR42. Clock
		* indices 0 and 1 should correspond to the VGA ones above,
		* and 3 is often 0 MHz, followed by extended clocks for a
		* total of mostly 16.
	*/
	
    if (modetiming->flags & USEPROGRCLOCK)
		moderegs[VGA_MISCOUTPUT] |= 0x0C;	/* External clock select. */
    else if (modetiming->selectedClockNo < 2) {
		/* Program clock select bits 0 and 1. */
		moderegs[VGA_MISCOUTPUT] &= ~0x0C;
		moderegs[VGA_MISCOUTPUT] |=
			(modetiming->selectedClockNo & 3) << 2;
    } else if (modetiming->selectedClockNo >= 2) {
		moderegs[VGA_MISCOUTPUT] |= 0x0C;
		/* Program S3 clock select bits. */
		moderegs[S3_CR42] &= ~0x1F;
		moderegs[S3_CR42] |=
			modetiming->selectedClockNo;
    }
    if (s3_chiptype == S3_TRIO64 || s3_chiptype == S3_765) {
		moderegs[S3_CR33] &= ~0x08;
		if (modeinfo->bitsPerPixel == 16)
			moderegs[S3_CR33] |= 0x08;
			/*
			* The rest of the DAC/clocking is setup by the
			* Trio64 code in the RAMDAC interface (ramdac.c).
		*/
    }
    /*if (dac_used->id != NORMAL_DAC) {
	int colormode;
	colormode = __svgalib_colorbits_to_colormode(modeinfo->bitsPerPixel,
	modeinfo->colorBits);
	dac_used->initializeState(&moderegs[S3_DAC_OFFSET],
	modeinfo->bitsPerPixel, colormode,
	modetiming->pixelClock);
	
	  if (dac_used->id == ATT20C490) {
	  int pixmux, invert_vclk, blank_delay;
	  pixmux = 0;
	  invert_vclk = 0;
	  blank_delay = 2;
	  if (colormode == CLUT8_6
	  && modetiming->pixelClock >= 67500) {
	  pixmux = 0x00;
	  invert_vclk = 1;
	  } else if (colormode == CLUT8_8)
	  pixmux = 0x02;
	  else if (colormode == RGB16_555)
	  pixmux = 0xa0;
	  else if (colormode == RGB16_565)
	  pixmux = 0xc0;
	  else if (colormode == RGB24_888_B)
	  pixmux = 0xe0;
	  moderegs[S3_CR67] = pixmux | invert_vclk;
	  moderegs[S3_CR6D] = blank_delay;
	}*/
	/*if (dac_used->id == S3_SDAC) {
	int pixmux, invert_vclk, blank_delay;
	pixmux = 0;
	invert_vclk = 0;
	blank_delay = 0;
	if (colormode == CLUT8_6
	&& modetiming->pixelClock >= 67500) {
#ifdef SDAC_8BPP_PIXMUX*/
	/* x64 8bpp pixel multiplexing? */
	/*		pixmux = 0x10;
	if (s3_chiptype != S3_866 && s3_chiptype != S3_868)
	invert_vclk = 1;
	blank_delay = 2;
	#endif
	} else if (colormode == RGB16_555) {
	pixmux = 0x30;
	blank_delay = 2;
	} else if (colormode == RGB16_565) {
	pixmux = 0x50;
	blank_delay = 2;
	} else if (colormode == RGB24_888_B) {	// XXXX 868/968 only
	pixmux = 0x90;
	blank_delay = 2;
	} else if (colormode == RGB32_888_B) {
	pixmux = 0x70;
	blank_delay = 2;
	}
	moderegs[S3_CR67] = pixmux | invert_vclk;
	moderegs[S3_CR6D] = blank_delay;
	// Clock select.
	moderegs[S3_CR42] &= ~0x0F;
	moderegs[S3_CR42] |= 0x02;
	}*/
	/*	if (dac_used->id == IBMRGB52x) {
	unsigned char pixmux, blank_delay, tmp;
	tmp = 0;
	pixmux = 0x11;
	blank_delay = 0;
	if (modeinfo->bitsPerPixel < 8 || colormode == RGB32_888_B)
	pixmux = 0x00;
	moderegs[S3_CR58] |= 0x40;
	moderegs[S3_CR65] = 0;
	moderegs[S3_CR66] &= 0xf8;
	moderegs[S3_CR66] |= tmp;
	#ifdef PIXEL_MULTIPLEXING
	moderegs[S3_CR67] = pixmux;
	#endif
	moderegs[S3_CR6D] = blank_delay;
	// Clock select.
	moderegs[S3_CR42] &= ~0x0F;
	moderegs[S3_CR42] |= 0x02;
	}
    }*/
#ifdef S3_LINEAR_SUPPORT
    s3_cr58 = moderegs[S3_CR58];
    s3_cr40 = moderegs[S3_CR40];
    s3_cr54 = moderegs[S3_CR54];
#endif
    /*if (clk_used == &__svgalib_I2061A_clockchip_methods &&
	(modetiming->flags & USEPROGRCLOCK)) {
	// Clock select.
	moderegs[S3_CR42] &= ~0x0F;
	moderegs[S3_CR42] |= 0x02;
    }*/
    /* update the 8514 regs */
    memcpy(moderegs + S3_8514_OFFSET, s3_8514regs, S3_8514_COUNT * 2);
}

int __svgalib_setregs(const unsigned char *regs)
{
    int i;
	
    //if(__svgalib_novga) return 1;
	
    //if (__svgalib_chipset == EGA) {
	/* Enable graphics register modification */
	//port_out(0x00, GRA_E0);
	//port_out(0x01, GRA_E1);
    //}
    /* update misc output register */
    __svgalib_vga_outmisc(regs[MIS]);
	
    /* synchronous reset on */
    __svgalib_vga_outseq(0x00,0x01);
	
    /* write sequencer registers */
    __svgalib_vga_outseq(0x01,regs[SEQ + 1] | 0x20);
    out(SEQ_I, 1);
    out(SEQ_D, regs[SEQ + 1] | 0x20);
    for (i = 2; i < SEQ_C; i++) {
		__svgalib_vga_outseq(i,regs[SEQ + i]);
    }
	
    /* synchronous reset off */
    __svgalib_vga_outseq(0x00,0x03);
	
    /*if (__svgalib_chipset != EGA) {
	// deprotect CRT registers 0-7
	__svgalib_outcrtc(0x11,__svgalib_incrtc(0x11)&0x7f);
}*/
    /* write CRT registers */
    for (i = 0; i < CRT_C; i++) {
        __svgalib_vga_outcrtc(i,regs[CRT + i]);
    }
	
    /* write graphics controller registers */
    for (i = 0; i < GRA_C; i++) {
		out(GRA_I, i);
		out(GRA_D, regs[GRA + i]);
    }
	
    /* write attribute controller registers */
    for (i = 0; i < ATT_C; i++) {
		in(__svgalib_IS1_R);		/* reset flip-flop */
		//__svgalib_delay();
		msleep(1);
		out(ATT_IW, i);
		//__svgalib_delay();
		msleep(1);
		out(ATT_IW, regs[ATT + i]);
		//__svgalib_delay();
		msleep(1);
    }
	
    return 0;
}

/* Set chipset-specific registers */
void CS3Graphics::SetRegisters(const unsigned char regs[], int mode)
{
    unsigned char b, bmax;
    /*
	* Right now, anything != 0x00 gets written in s3_setregs.
	* May change this into a bitmask later.
	*/
    static unsigned char s3_regmask[] =
    {
		0x00, 0x31, 0x32, 0x33, 0x34, 0x35, 0x00, 0x00,		/* CR30-CR37 */
			0x00, 0x00, 0x3A, 0x3B, 0x3C, 0x00, 0x00, 0x00,		/* CR38-CR3F */
			0x00, 0x00, 0x42, 0x43, 0x44, 0x00, 0x00, 0x00,		/* CR40-CR47 */
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* CR48-CR4F */
			0x50, 0x51, 0x00, 0x00, 0x54, 0x55, 0x00, 0x00,		/* CR50-CR57 */
			0x58, 0x59, 0x5A, 0x00, 0x00, 0x5D, 0x5E, 0x00,		/* CR58-CR5F */
			0x60, 0x61, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00,		/* CR60-CR67 */
			0x00, 0x00, 0x6A, 0x00, 0x00, 0x00			/* CR68-CR6D */
    };
	
    HwUnlockEnh();
	
    /* save a private copy */
    memcpy(s3_8514regs, regs + S3_8514_OFFSET, S3_8514_COUNT * 2);
    /*
	* set this first, so if we segfault on this (e.g. we didn't do a iopl(3))
	* we don't get a screwed up display
	*/
    out16(ADVFUNC_CNTL, s3_8514regs[S3_ADVFUNC_CNTL]);
	
    /* get S3 VGA/Ext registers */
    bmax = 0x4F;
    if (s3_chiptype >= S3_801)
		bmax = 0x66;
    if (s3_chiptype >= S3_864)
		bmax = 0x6D;
    for (b = 0x30; b <= bmax; b++) {
		if (s3_regmask[b - 0x30])
			__svgalib_outCR(b, regs[EXT + b - 0x30]);
    }
	
    /*if (dac_used->id != NORMAL_DAC) {
	unsigned char CR1;
	// Blank the screen.
	CR1 = __svgalib_inCR(0x01);
	__svgalib_outCR(0x01, CR1 | 0x20);
	
	  __svgalib_outbCR(0x55, __svgalib_inCR(0x55) | 1);
	  __svgalib_outCR(0x66, regs[S3_CR66]);
	  __svgalib_outCR(0x67, regs[S3_CR67]);	// S3 pixmux.
	  
		dac_used->restoreState(regs + S3_DAC_OFFSET);
		
		  __svgalib_outCR(0x6D, regs[S3_CR6D]);
		  __svgalib_outbCR(0x55, __svgalib_inCR(0x55) & ~1);
		  
			__svgalib_outCR(0x01, CR1);	// Unblank screen.
}*/
#ifdef S3_LINEAR_SUPPORT
    if (mode == TEXT && s3_linear_addr)
		s3_linear_disable();	/* make sure linear is off */
#endif
	
    /* restore CR38/39 (may lock other regs) */
    if (mode == 1) {
		/* restore lock registers as well */
		__svgalib_outCR(0x40, regs[S3_CR40]);
		__svgalib_outCR(0x39, regs[S3_CR39]);
		__svgalib_outCR(0x38, regs[S3_CR38]);
    } else
		HwLockEnh();
}

/* Set a mode */

int CS3Graphics::SetMode(int mode, int prv_mode)
{
    ModeInfo *modeinfo;
    ModeTiming *modetiming;
    unsigned char moderegs[S3_TOTAL_REGS];
    int res;
	
    /*if (mode < G640x480x256 || mode == G720x348x2) {
	// Let the standard VGA driver set standard VGA modes.
	res = __svgalib_vga_driverspecs.setmode(mode, prv_mode);
	if (res == 0 && s3_chiptype <= S3_928) */
	/*
	* ARI: Turn off virtual size of 1024 - this fixes all problems
	*      with standard modes, including 320x200x256.
	* 
	* SL:  Is this for 928 only?  Doesn't matter for 805.
	*/
	/*HwUnlock();
	__svgalib_outCR(0x34, __svgalib_inCR(0x34) & ~0x10);
	HwLock();
	}
	return res;
}*/
    if (!ModeAvailable(mode))
	{
		wprintf(L"Mode %d not available\n", mode);
		return 1;
	}
	
    modeinfo = __svgalib_createModeInfoStructureForSvgalibMode(mode);
	wprintf(L"Mode: %ux%ux%u\n", modeinfo->width, modeinfo->height, modeinfo->bitsPerPixel);
	
    modetiming = (ModeTiming*) malloc(sizeof(ModeTiming));
    if (__svgalib_getmodetiming(modetiming, modeinfo, cardspecs))
	{
		wprintf(L"Unable to get mode timings\n");
		free(modetiming);
		free(modeinfo);
		return 1;
    }
    /* Adjust the display width. */
    modeinfo->lineWidth = AdjustLineWidth(modeinfo->lineWidth);
    //CI.xbytes = modeinfo->lineWidth;
	
    InitializeMode(moderegs, modetiming, modeinfo);
    free(modeinfo);
    free(modetiming);
	
    __svgalib_setregs(moderegs);	/* Set standard regs. */
    SetRegisters(moderegs, mode);	/* Set extended regs. */
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CS3Graphics::CS3Graphics()
{
	s3Mclk = 0;
	Init(0, 0, 0);
	if (SetMode(0, 0))
		wprintf(L"Unable to set mode\n");
}

CS3Graphics::~CS3Graphics()
{
	
}

HRESULT CS3Graphics::SetPalette(int nIndex, int red, int green, int blue)
{
	return E_FAIL;
}

HRESULT CS3Graphics::Lock(surface_t* pDesc)
{
	return E_FAIL;
}

HRESULT CS3Graphics::Unlock()
{
	return E_FAIL;
}

HRESULT CS3Graphics::GetSurfaceDesc(surface_t* pDesc)
{
	return E_FAIL;
}

pixel_t CS3Graphics::ColourMatch(colour_t clr)
{
	return 0;
}

HRESULT CS3Graphics::SetPixel(int x, int y, pixel_t pix)
{
	return E_FAIL;
}

pixel_t CS3Graphics::GetPixel(int x, int y)
{
	return 0;
}

HRESULT CS3Graphics::Blt(ISurface* pSrc, int x, int y, int nWidth,
						 int nHeight, int nSrcX, int nSrcY, pixel_t pixTrans)
{
	return E_FAIL;
}

HRESULT CS3Graphics::FillRect(const rectangle_t* rect, pixel_t pix)
{
	return E_FAIL;
}

HRESULT CS3Graphics::AttachProcess()
{
	return S_OK;
}