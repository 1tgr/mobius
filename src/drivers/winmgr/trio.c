/*
 * Trio64 driver for x86. Most of this code originates from Amiga Cybervision
 * driver, and some from the Amiga S3 driver, but has been modified to work 
 * in a x86/PCI environment, and after PC BIOS has trashed several registers 
 * Cybervision apparently doesn't need to touch. Slight code cleanup has
 * been done, and a rewrite of the PLL value calculation routine. 
 * Initially I wanted to do a platform-independent version of the Cybervision
 * driver, but the debugging process got too complicated (and I don't have
 * an Amiga to test it), so I ended up with a x86-only version. 
 *
 * So far, acceleration works only through programmed i/o, which means no
 * memory mapping of acceleration engine registers, which means no support
 * for userland graphics acceleration, which actually sucks. As far as my
 * documentation tells me, Trio supports mmio in a fixed memory area (from
 * 0xA8000 to 0xAFFFF), but apparently (reading from the sources) the 
 * Cybervision implementation behaves differently. Anyone willing to
 * clarify this?
 *
 * Version: 0.1 (initial release), released 1999-09-17
 * BUGS/TODO:
 *  - no mmaping of acceleration registers
 *  - poor support for 16 bpp, no support for 24bpp
 *  - trio32 support
 *  - modularization
 *  - virge support (and rename the driver ;-) ?
 *  - hwcursor support?
 *  - more options?
 *  - where does that white bg come from in the beginning?
 *    I have a hunch fbcon does something I'm not prepared for...
 *
 * -- Hannu Mallat <hmallat@cs.hut.fi>
 */

/*#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/pci.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>*/

#define TRIO_ACCEL
#include <string.h>
#include <stdio.h>
#include "fb.h"
#include "driver.h"
#include "pci.h"

void __declspec(naked) _ftol()
{
	__asm
	{
		push        ebp
		mov         ebp,esp
		add         esp,0F4h
		wait
		fnstcw      word ptr [ebp-2]
		wait
		mov         ax,word ptr [ebp-2]
		or          ah,0Ch
		mov         word ptr [ebp-4],ax
		fldcw       word ptr [ebp-4]
		fistp       qword ptr [ebp-0Ch]
		fldcw       word ptr [ebp-2]
		mov         eax,dword ptr [ebp-0Ch]
		mov         edx,dword ptr [ebp-8]
		leave
		ret
	}
}

#define __initdata
#define writeb(val, addr)	(*((byte*) (addr)) = val)
#define PCI_VENDOR_ID_S3                0x5333
#define PCI_DEVICE_ID_S3_TRIO           0x8811
#define  PCI_BASE_ADDRESS_MEM_MASK      (~0x0fUL)
int _fltused = 0x9875;

/* s3 commands */
#define S3_BITBLT       0xc011
#define S3_TWOPOINTLINE 0x2811
#define S3_FILLEDRECT   0x40b1

/* Drawing modes */
#define S3_NOTCUR          0x0000
#define S3_LOGICALZERO     0x0001
#define S3_LOGICALONE      0x0002
#define S3_LEAVEASIS       0x0003
#define S3_NOTNEW          0x0004
#define S3_CURXORNEW       0x0005
#define S3_NOT_CURXORNEW   0x0006
#define S3_NEW             0x0007
#define S3_NOTCURORNOTNEW  0x0008
#define S3_CURORNOTNEW     0x0009
#define S3_NOTCURORNEW     0x000a
#define S3_CURORNEW        0x000b
#define S3_CURANDNEW       0x000c
#define S3_NOTCURANDNEW    0x000d
#define S3_CURANDNOTNEW    0x000e
#define S3_NOTCURANDNOTNEW 0x000f

/* stuff... */
#define S3_FIFO_EMPTY 0x0400
#define S3_HDW_BUSY   0x0200

#define S3_VIDEO_SUBS_ENABLE 0x46e8
#define S3_OPTION_SELECT     0x0102

#define S3_SUBSYS_CNTL   0x42e8
#define S3_ADVFUNC_CNTL  0x4ae8
#define S3_CUR_Y         0x82e8
#define S3_CUR_Y2	 0x82ea
#define S3_CUR_X         0x86e8
#define S3_CUR_X2	 0x86ea
#define S3_DESTY_AX      0x8ae8
#define S3_DESTY2_AX2	 0x8AEA
#define S3_DESTX_DIA     0x8ee8
#define S3_DESTX2_DIA2	 0x8EEA
#define S3_ERR_TERM      0x92e8
#define S3_READ_REG_DATA 0xbee8
#define S3_READ_SEL      0xbee8 /* offset f */
#define S3_MULT_MISC     0xbee8 /* offset e */
#define S3_MULT_MISC_2   0xbee8 /* offset d */
#define S3_FRGD_COLOR    0xa6e8
#define S3_BKGD_COLOR    0xa2e8
#define S3_PIXEL_CNTL    0xbee8 /* offset a */
#define S3_FRGD_MIX      0xbae8
#define S3_BKGD_MIX      0xb6e8
#define S3_DEST_Y_AX	 0x8AE8
#define S3_DEST_X_DIA	 0x8EE8
#define S3_MIN_AXIS_PCNT 0xbee8 /* offset 0 */
#define S3_MAJ_AXIS_PCNT 0x96e8
#define S3_CMD           0x9ae8
#define S3_GP_STAT       0x9ae8
#define S3_WRT_MASK      0xaae8
#define S3_RD_MASK       0xaee8
#define S3_SHORT_STROKE	 0x9EE8

/* General Registers: */
#define GREG_MISC_OUTPUT_R	0x03CC
#define GREG_MISC_OUTPUT_W	0x03C2	
#define GREG_FEATURE_CONTROL_R	0x03CA
#define GREG_FEATURE_CONTROL_W	0x03DA
#define GREG_INPUT_STATUS0_R	0x03C2
#define GREG_INPUT_STATUS1_R	0x03DA

/* Attribute Controller: */
#define ACT_ADDRESS		0x03C0
#define ACT_ADDRESS_R		0x03C1
#define ACT_ADDRESS_W		0x03C0
#define ACT_ADDRESS_RESET	0x03DA
#define ACT_ID_PALETTE0		0x00
#define ACT_ID_PALETTE1		0x01
#define ACT_ID_PALETTE2		0x02
#define ACT_ID_PALETTE3		0x03
#define ACT_ID_PALETTE4		0x04
#define ACT_ID_PALETTE5		0x05
#define ACT_ID_PALETTE6		0x06
#define ACT_ID_PALETTE7		0x07
#define ACT_ID_PALETTE8		0x08
#define ACT_ID_PALETTE9		0x09
#define ACT_ID_PALETTE10	0x0A
#define ACT_ID_PALETTE11	0x0B
#define ACT_ID_PALETTE12	0x0C
#define ACT_ID_PALETTE13	0x0D
#define ACT_ID_PALETTE14	0x0E
#define ACT_ID_PALETTE15	0x0F
#define ACT_ID_ATTR_MODE_CNTL	0x10
#define ACT_ID_OVERSCAN_COLOR	0x11
#define ACT_ID_COLOR_PLANE_ENA	0x12
#define ACT_ID_HOR_PEL_PANNING	0x13
#define ACT_ID_COLOR_SELECT	0x14

/* Graphics Controller: */
#define GCT_ADDRESS		0x03CE
#define GCT_ADDRESS_R		0x03CF
#define GCT_ADDRESS_W		0x03CF
#define GCT_ID_SET_RESET	0x00
#define GCT_ID_ENABLE_SET_RESET	0x01
#define GCT_ID_COLOR_COMPARE	0x02
#define GCT_ID_DATA_ROTATE	0x03
#define GCT_ID_READ_MAP_SELECT	0x04
#define GCT_ID_GRAPHICS_MODE	0x05
#define GCT_ID_MISC		0x06
#define GCT_ID_COLOR_XCARE	0x07
#define GCT_ID_BITMASK		0x08

/* Sequencer: */
#define SEQ_ADDRESS		0x03C4
#define SEQ_ADDRESS_R		0x03C5
#define SEQ_ADDRESS_W		0x03C5
#define SEQ_ID_RESET		0x00
#define SEQ_ID_CLOCKING_MODE	0x01
#define SEQ_ID_MAP_MASK		0x02
#define SEQ_ID_CHAR_MAP_SELECT	0x03
#define SEQ_ID_MEMORY_MODE	0x04
#define SEQ_ID_UNKNOWN1		0x05
#define SEQ_ID_UNKNOWN2		0x06
#define SEQ_ID_UNKNOWN3		0x07
/* S3 extensions */
#define SEQ_ID_UNLOCK_EXT	0x08
#define SEQ_ID_EXT_SEQ_REG9	0x09
#define SEQ_ID_BUS_REQ_CNTL	0x0A
#define SEQ_ID_EXT_MISC_SEQ	0x0B
#define SEQ_ID_UNKNOWN4		0x0C
#define SEQ_ID_EXT_SEQ		0x0D
#define SEQ_ID_UNKNOWN5		0x0E
#define SEQ_ID_UNKNOWN6		0x0F
#define SEQ_ID_MCLK_LO		0x10
#define SEQ_ID_MCLK_HI		0x11
#define SEQ_ID_DCLK_LO		0x12
#define SEQ_ID_DCLK_HI		0x13
#define SEQ_ID_CLKSYN_CNTL_1	0x14
#define SEQ_ID_CLKSYN_CNTL_2	0x15
#define SEQ_ID_CLKSYN_TEST_HI	0x16	/* reserved for S3 testing of the */
#define SEQ_ID_CLKSYN_TEST_LO	0x17	/*   internal clock synthesizer   */
#define SEQ_ID_RAMDAC_CNTL	0x18
#define SEQ_ID_MORE_MAGIC	0x1A

/* CRT Controller: */
#define CRT_ADDRESS		0x03D4
#define CRT_ADDRESS_R		0x03D5
#define CRT_ADDRESS_W		0x03D5
#define CRT_ID_HOR_TOTAL	0x00
#define CRT_ID_HOR_DISP_ENA_END	0x01
#define CRT_ID_START_HOR_BLANK	0x02
#define CRT_ID_END_HOR_BLANK	0x03
#define CRT_ID_START_HOR_RETR	0x04
#define CRT_ID_END_HOR_RETR	0x05
#define CRT_ID_VER_TOTAL	0x06
#define CRT_ID_OVERFLOW		0x07
#define CRT_ID_PRESET_ROW_SCAN	0x08
#define CRT_ID_MAX_SCAN_LINE	0x09
#define CRT_ID_CURSOR_START	0x0A
#define CRT_ID_CURSOR_END	0x0B
#define CRT_ID_START_ADDR_HIGH	0x0C
#define CRT_ID_START_ADDR_LOW	0x0D
#define CRT_ID_CURSOR_LOC_HIGH	0x0E
#define CRT_ID_CURSOR_LOC_LOW	0x0F
#define CRT_ID_START_VER_RETR	0x10
#define CRT_ID_END_VER_RETR	0x11
#define CRT_ID_VER_DISP_ENA_END	0x12
#define CRT_ID_SCREEN_OFFSET	0x13
#define CRT_ID_UNDERLINE_LOC	0x14
#define CRT_ID_START_VER_BLANK	0x15
#define CRT_ID_END_VER_BLANK	0x16
#define CRT_ID_MODE_CONTROL	0x17
#define CRT_ID_LINE_COMPARE	0x18
#define CRT_ID_GD_LATCH_RBACK	0x22
#define CRT_ID_ACT_TOGGLE_RBACK	0x24
#define CRT_ID_ACT_INDEX_RBACK	0x26
/* S3 extensions: S3 VGA Registers */
#define CRT_ID_DEVICE_HIGH	0x2D
#define CRT_ID_DEVICE_LOW	0x2E
#define CRT_ID_REVISION 	0x2F
#define CRT_ID_CHIP_ID_REV	0x30
#define CRT_ID_MEMORY_CONF	0x31
#define CRT_ID_BACKWAD_COMP_1	0x32
#define CRT_ID_BACKWAD_COMP_2	0x33
#define CRT_ID_BACKWAD_COMP_3	0x34
#define CRT_ID_REGISTER_LOCK	0x35
#define CRT_ID_CONFIG_1 	0x36
#define CRT_ID_CONFIG_2 	0x37
#define CRT_ID_REGISTER_LOCK_1	0x38
#define CRT_ID_REGISTER_LOCK_2	0x39
#define CRT_ID_MISC_1		0x3A
#define CRT_ID_DISPLAY_FIFO	0x3B
#define CRT_ID_LACE_RETR_START	0x3C
/* S3 extensions: System Control Registers  */
#define CRT_ID_SYSTEM_CONFIG	0x40
#define CRT_ID_BIOS_FLAG	0x41
#define CRT_ID_LACE_CONTROL	0x42
#define CRT_ID_EXT_MODE 	0x43
#define CRT_ID_HWGC_MODE	0x45	/* HWGC = Hardware Graphics Cursor */
#define CRT_ID_HWGC_ORIGIN_X_HI	0x46
#define CRT_ID_HWGC_ORIGIN_X_LO	0x47
#define CRT_ID_HWGC_ORIGIN_Y_HI	0x48
#define CRT_ID_HWGC_ORIGIN_Y_LO	0x49
#define CRT_ID_HWGC_FG_STACK	0x4A
#define CRT_ID_HWGC_BG_STACK	0x4B
#define CRT_ID_HWGC_START_AD_HI	0x4C
#define CRT_ID_HWGC_START_AD_LO	0x4D
#define CRT_ID_HWGC_DSTART_X	0x4E
#define CRT_ID_HWGC_DSTART_Y	0x4F
/* S3 extensions: System Extension Registers  */
#define CRT_ID_EXT_SYS_CNTL_1	0x50
#define CRT_ID_EXT_SYS_CNTL_2	0x51
#define CRT_ID_EXT_BIOS_FLAG_1	0x52
#define CRT_ID_EXT_MEM_CNTL_1	0x53
#define CRT_ID_EXT_MEM_CNTL_2	0x54
#define CRT_ID_EXT_DAC_CNTL	0x55
#define CRT_ID_EX_SYNC_1	0x56
#define CRT_ID_EX_SYNC_2	0x57
#define CRT_ID_LAW_CNTL		0x58	/* LAW = Linear Address Window */
#define CRT_ID_LAW_POS_HI	0x59
#define CRT_ID_LAW_POS_LO	0x5A
#define CRT_ID_GOUT_PORT	0x5C
#define CRT_ID_EXT_HOR_OVF	0x5D
#define CRT_ID_EXT_VER_OVF	0x5E
#define CRT_ID_EXT_MEM_CNTL_3	0x60
#define CRT_ID_EX_SYNC_3	0x63
#define CRT_ID_EXT_MISC_CNTL	0x65
#define CRT_ID_EXT_MISC_CNTL_1	0x66
#define CRT_ID_EXT_MISC_CNTL_2	0x67
#define CRT_ID_CONFIG_3 	0x68
#define CRT_ID_EXT_SYS_CNTL_3	0x69
#define CRT_ID_EXT_SYS_CNTL_4	0x6A
#define CRT_ID_EXT_BIOS_FLAG_3	0x6B
#define CRT_ID_EXT_BIOS_FLAG_4	0x6C

/* Video DAC */
#define VDAC_ADDRESS		0x03c8
#define VDAC_ADDRESS_W		0x03c8
#define VDAC_ADDRESS_R		0x03c7
#define VDAC_STATE		0x03c7
#define VDAC_DATA		0x03c9
#define VDAC_MASK		0x03c6

#define TRIO_ACCEL 
/* #define TRIO_MMIO */ /* does not compute */

static wchar_t triofb_name[16] = L"S3 Trio 64";

static unsigned char Trio_colour_table [256][3];
static unsigned long TrioMem      = 0;
static unsigned long TrioSize     = 0;
static unsigned long TrioMem_phys = 0;
#ifdef TRIO_MMIO
static unsigned long TrioRegs     = 0;
#endif /* TRIO_MMIO */

long trio_memclk = 45000000; /* default (?) */

//#define vga_inb(reg)     inb(reg)
//#define vga_outb(reg,dat) outb((dat) & 0xff, reg)
#define vga_inb(reg)		in(reg)
#define vga_outb(reg, dat)	out(reg, dat)

#ifdef TRIO_MMIO
void trio_outw(unsigned long idx, unsigned short val) {
  writew(val, TrioRegs + idx); 
}
unsigned short trio_inw(unsigned long idx) {
  return readw(TrioRegs + idx);
}
#else /* TRIO_MMIO */
/*void trio_outw(unsigned short idx, unsigned short val) {
  __asm__ volatile ("outw %0,%1"
		    ::"a" ((unsigned short) val), 
		    "d"((unsigned short) idx));
}
unsigned short trio_inw(unsigned short idx) {
  unsigned short val;
  __asm__ volatile ("inw %1,%0"
		    :"=a" (val)
		    :"d"((unsigned short) idx));
  return val;
}*/

#define trio_outw(idx, val)	out16(idx, val)
#define trio_inw(idx)		in16(idx)
#endif /* TRIO_MMIO */

void gra_outb(unsigned short idx, unsigned char val) {
  vga_outb(GCT_ADDRESS,   idx); 
  vga_outb(GCT_ADDRESS_W, val);
}

void seq_outb(unsigned short idx, unsigned char val) {
  vga_outb(SEQ_ADDRESS,   idx); 
  vga_outb(SEQ_ADDRESS_W, val);
}

void crt_outb(unsigned short idx, unsigned char val) {
  vga_outb(CRT_ADDRESS,   idx); 
  vga_outb(CRT_ADDRESS_W, val);
}

void att_outb(unsigned short idx, unsigned char val) {
  unsigned char tmp;
  tmp = vga_inb(ACT_ADDRESS_RESET);
  vga_outb(ACT_ADDRESS_W, idx);
  vga_outb(ACT_ADDRESS_W, val);
} 

unsigned char att_inb(unsigned short idx) {
  vga_outb(ACT_ADDRESS_W, idx);
  msleep(1); //udelay(100);
  return vga_inb(ACT_ADDRESS_R);
}

unsigned char seq_inb(unsigned short idx) {
  vga_outb(SEQ_ADDRESS, idx);
  return vga_inb(SEQ_ADDRESS_R);
}

unsigned char crt_inb(unsigned short idx) {
  vga_outb(CRT_ADDRESS, idx);
  return vga_inb(CRT_ADDRESS_R);
}

unsigned char gra_inb(unsigned short idx) {
  vga_outb(GCT_ADDRESS, idx);
  return vga_inb(GCT_ADDRESS_R);
}

struct triofb_par {
  struct fb_var_screeninfo var;
  dword type;
  dword type_aux;
  dword visual;
  dword line_length;
};

static struct triofb_par current_par;

static int current_par_valid = 0;
static int currcon = 0;

//static struct display disp;
//static struct fb_info fb_info;

static struct fb_hwswitch {
  int (*init)(void);

  int (*encode_fix)(struct fb_fix_screeninfo *fix, struct triofb_par *par);
  int (*decode_var)(struct fb_var_screeninfo *var, struct triofb_par *par);
  int (*encode_var)(struct fb_var_screeninfo *var, struct triofb_par *par);
  int (*getcolreg)(dword regno, dword *red, dword *green, dword *blue,
		   dword *transp, struct fb_info *info);
  int (*setcolreg)(dword regno, dword red, dword green, dword blue,
		   dword transp, struct fb_info *info);
  void (*blank)(int blank);
} *fbhw;

struct fb_videomode
{
	wchar_t* name;
	struct fb_var_screeninfo var;
};

static struct fb_videomode triofb_predefined[] __initdata = {
  { L"640x480-8", {		/* Default 8 BPP mode (trio8) */
    640, 480, 640, 480, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, FB_ACCELF_TEXT, 39722, 40, 24, 32, 11, 96, 2,
    FB_SYNC_COMP_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, 
    FB_VMODE_NONINTERLACED
  }}, 
  { L"640x480-16", {		/* Default 16 BPP mode (trio16) */
    640, 480, 640, 480, 0, 0, 16, 0,
    {11, 5, 0}, {5, 6, 0}, {0, 5, 0}, {0, 0, 0},
    0, 0, -1, -1, FB_ACCELF_TEXT, 39722, 40, 24, 32, 11, 96, 2,
    FB_SYNC_COMP_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, 
    FB_VMODE_NONINTERLACED
  }}, 
  { L"800x600-8", {		/* Trio 8 bpp */
    800, 600, 800, 600, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, FB_ACCELF_TEXT, 27778, 64, 24, 22, 1, 72, 2,
    FB_SYNC_COMP_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
    FB_VMODE_NONINTERLACED
  }},
  { L"1024x768-8", {		/* Trio 8 bpp */
    1024, 768, 1024, 768, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, FB_ACCELF_TEXT, 16667, 224, 72, 60, 12, 168, 4,
    FB_SYNC_COMP_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
    FB_VMODE_NONINTERLACED
  }}
};

#define NUM_TOTAL_MODES sizeof(triofb_predefined)/sizeof(struct fb_videomode)

#define TRIO8_DEFMODE     (0)
#define TRIO16_DEFMODE    (1)

static struct fb_var_screeninfo triofb_default = { 
  /* Default 8 BPP mode (trio8) */
  640, 480, 640, 480, 0, 0, 8, 0,
  {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
  0, 0, -1, -1, FB_ACCELF_TEXT, 39722, 40, 24, 32, 11, 96, 2,
  FB_SYNC_COMP_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, 
  FB_VMODE_NONINTERLACED
}; 

static int Triofb_inverse = 0;

void triofb_setup(char *options, int *ints);

static int triofb_open(struct fb_info *info, int user);
static int triofb_release(struct fb_info *info, int user);
static int triofb_get_fix(struct fb_fix_screeninfo *fix, int con,
			   struct fb_info *info);
static int triofb_get_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info);
static int triofb_set_var(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info);
static int triofb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info);
static int triofb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info);
static int triofb_pan_display(struct fb_var_screeninfo *var, int con,
			       struct fb_info *info);
static int triofb_ioctl(struct inode *inode, struct file *file, dword cmd,
			 dword arg, int con, struct fb_info *info);

void triofb_init(void);
static int Triofb_switch(int con, struct fb_info *info);
static int Triofb_updatevar(int con, struct fb_info *info);
static void Triofb_blank(int blank, struct fb_info *info);

static int Trio_init(void);
static int Trio_encode_fix(struct fb_fix_screeninfo *fix,
			    struct triofb_par *par);
static int Trio_decode_var(struct fb_var_screeninfo *var,
			    struct triofb_par *par);
static int Trio_encode_var(struct fb_var_screeninfo *var,
			    struct triofb_par *par);
static int Trio_getcolreg(dword regno, dword *red, dword *green, dword *blue,
			   dword *transp, struct fb_info *info);
static int Trio_setcolreg(dword regno, dword red, dword green, dword blue,
			   dword transp, struct fb_info *info);
static void Trio_blank(int blank);

static void triofb_get_par(struct triofb_par *par);
static void triofb_set_par(struct triofb_par *par);
static int do_fb_set_var(struct fb_var_screeninfo *var, int isactive);
static void do_install_cmap(int con, struct fb_info *info);
static void triofb_set_disp(int con, struct fb_info *info);
static int get_video_mode(const char *name);

static unsigned short trio_compute_clock(unsigned long);
static void trio_load_video_mode (struct fb_var_screeninfo *);
inline void trio_video_disable(int toggle);

#ifdef TRIO_ACCEL
static void Trio_WaitQueue(void);
static void Trio_WaitBlit(void);
static void Trio_WaitIdle(void);
static void Trio_BitBLT(word curx, word cury, word destx,
			 word desty, word width, word height,
			 word mode);
static void Trio_RectFill(word x, word y, word width, word height,
			  word color);
static void Trio_MoveCursor(word x, word y);
#ifdef FBCON_HAS_CFB8
static struct display_switch fbcon_trio8;
#endif /* FBCON_HAS_CFB8 */
#endif /* TRIO_ACCEL */

static int Trio_init(void) {
  int i;
  unsigned char test;
  unsigned int clockpar;
  volatile dword *CursorBase;

  /* make sure 0x46e8 accesses are responded to */
  crt_outb(CRT_ID_EXT_MISC_CNTL, crt_inb(CRT_ID_EXT_MISC_CNTL) & 0xfb);

  vga_outb(S3_VIDEO_SUBS_ENABLE, 0x10);
  vga_outb(S3_OPTION_SELECT,     0x01);
  vga_outb(S3_VIDEO_SUBS_ENABLE, 0x08);

  out16(S3_SUBSYS_CNTL, 0x8000); in16(S3_SUBSYS_CNTL);/* reset accelerator */
  out16(S3_SUBSYS_CNTL, 0x4000); in16(S3_SUBSYS_CNTL);/* enable accelerator */

#ifdef TRIO_MMIO
  out16(S3_ADVFUNC_CNTL, 0x0031);
  crt_outb(CRT_ID_EXT_MEM_CNTL_1, 0x18);
#else /* TRIO_MMIO */
  out16(S3_ADVFUNC_CNTL, 0x0011);
  crt_outb(CRT_ID_EXT_MEM_CNTL_1, 0x00);
#endif /* TRIO_MMIO */

  Trio_WaitIdle();
  out16(0xe000, S3_MULT_MISC);
  out16(0xd000, S3_MULT_MISC_2);

  if(TrioSize == 4096*1024) {
    crt_outb(CRT_ID_LAW_CNTL, 0x13);
  } else {
    crt_outb(CRT_ID_LAW_CNTL, 0x12);
  }
	
  seq_outb(SEQ_ID_CLOCKING_MODE,   0x01);
  seq_outb(SEQ_ID_MAP_MASK,        0x0f);
  seq_outb(SEQ_ID_CHAR_MAP_SELECT, 0x00);
  seq_outb(SEQ_ID_MEMORY_MODE,     0x0e);

  seq_outb(SEQ_ID_EXT_SEQ_REG9,    0x00);
  if((crt_inb(0x36) & 0x0c) == 0x0c && /* fast page mode */
     (crt_inb(0x36) & 0xe0) == 0x00) { /* 4Mb buffer */
    seq_outb(SEQ_ID_BUS_REQ_CNTL, seq_inb(SEQ_ID_BUS_REQ_CNTL) | 0x40);
  }

  /* Clear immediate clock load bit */
  test = seq_inb(SEQ_ID_CLKSYN_CNTL_2);
  test = test & 0xDF;
  /* If > 55MHz, enable 2 cycle memory write */
  if (trio_memclk >= 55000000) {
    test |= 0x80;
  }
  seq_outb(SEQ_ID_CLKSYN_CNTL_2, test);

  /* Set MCLK value */
  clockpar = trio_compute_clock (trio_memclk);
  test = (clockpar & 0xFF00) >> 8;
  seq_outb(SEQ_ID_MCLK_HI, test);
  test = clockpar & 0xFF;
  seq_outb(SEQ_ID_MCLK_LO, test);

  /* Chip rev specific: Not in my Trio manual!!! */
  if(crt_inb(CRT_ID_REVISION) == 0x10)
    seq_outb(SEQ_ID_MORE_MAGIC, test);

  /* Set DCLK value */
  seq_outb(SEQ_ID_DCLK_HI, 0x13);
  seq_outb(SEQ_ID_DCLK_LO, 0x41);

  /* Load DCLK (and MCLK?) immediately */
  test = seq_inb(SEQ_ID_CLKSYN_CNTL_2);
  test = test | 0x22;
  seq_outb(SEQ_ID_CLKSYN_CNTL_2, test);

  /* Enable loading of DCLK */
  test = vga_inb(GREG_MISC_OUTPUT_R);
  test = test | 0x0C;
  vga_outb(GREG_MISC_OUTPUT_W, test);

  /* Turn off immediate xCLK load */
  seq_outb(SEQ_ID_CLKSYN_CNTL_2, 0x2);

  gra_outb(GCT_ID_SET_RESET, 0x0);
  gra_outb(GCT_ID_ENABLE_SET_RESET, 0x0);
  gra_outb(GCT_ID_COLOR_COMPARE, 0x0);
  gra_outb(GCT_ID_DATA_ROTATE, 0x0);
  gra_outb(GCT_ID_READ_MAP_SELECT, 0x0);
  gra_outb(GCT_ID_GRAPHICS_MODE, 0x40);
  gra_outb(GCT_ID_MISC, 0x01);
  gra_outb(GCT_ID_COLOR_XCARE, 0x0F);
  gra_outb(GCT_ID_BITMASK, 0xFF);
	
  /* Colors for text mode */
  for (i = 0; i < 0xf; i++) att_outb(i, i);
  att_outb(ACT_ID_ATTR_MODE_CNTL, 0x41);
  att_outb(ACT_ID_OVERSCAN_COLOR, 0x01);
  att_outb(ACT_ID_COLOR_PLANE_ENA, 0x0F);
  att_outb(ACT_ID_HOR_PEL_PANNING, 0x0);
  att_outb(ACT_ID_COLOR_SELECT, 0x0);
  vga_outb(VDAC_MASK, 0xFF);
	
  /* Colors initially set to grayscale */	
  vga_outb(VDAC_ADDRESS_W, 0);
  for (i = 255; i >= 0; i--) {
    vga_outb(VDAC_DATA, i);
    vga_outb(VDAC_DATA, i);
    vga_outb(VDAC_DATA, i);
  }

  crt_outb(CRT_ID_BACKWAD_COMP_3, 0x10);	/* FIFO enabled */
  crt_outb(CRT_ID_HWGC_MODE, 0x00); /* GFx hardware cursor off */
  crt_outb(CRT_ID_HWGC_DSTART_X, 0x00);
  crt_outb(CRT_ID_HWGC_DSTART_Y, 0x00);
  att_outb(0x33, 0);
  trio_video_disable(0);  

  /* Init local cmap as greyscale levels */
  for (i = 0; i < 256; i++) {
    Trio_colour_table [i][0] = i;
    Trio_colour_table [i][1] = i;
    Trio_colour_table [i][2] = i;
  }

  /* Clear framebuffer memory */
  for(i = 0; i < TrioSize; i++) writeb(0, TrioMem + i);

  /* Initialize hardware cursor */
  CursorBase = (dword *)((char *)(TrioMem) + TrioSize - 0x400);
#if 0
  for (i=0; i < 8; i++) {
    *(CursorBase  +(i*4)) = 0xffffff00;
    *(CursorBase+1+(i*4)) = 0xffff0000;
    *(CursorBase+2+(i*4)) = 0xffff0000;
    *(CursorBase+3+(i*4)) = 0xffff0000;
  }
  for (i=8; i < 64; i++) {
    *(CursorBase  +(i*4)) = 0xffff0000;
    *(CursorBase+1+(i*4)) = 0xffff0000;
    *(CursorBase+2+(i*4)) = 0xffff0000;
    *(CursorBase+3+(i*4)) = 0xffff0000;
  }
  Trio_setcolreg (255, 56<<8, 100<<8, 160<<8, 0, NULL /* unused */);
  Trio_setcolreg (254, 0, 0, 0, 0, NULL /* unused */);
#endif
  
  return 0;
}

static int Trio_encode_fix(struct fb_fix_screeninfo *fix,
			   struct triofb_par *par) {
  memset(fix, 0, sizeof(struct fb_fix_screeninfo));
  wcscpy(fix->id, triofb_name);
  fix->smem_start = (char*)TrioMem_phys;
  fix->smem_len   = TrioSize;
#ifdef TRIO_MMIO
  fix->mmio_start = (char*)TrioRegs;
  fix->mmio_len   = 0x10000;
#else /* TRIO_MMIO */
  fix->mmio_start = 0;
  fix->mmio_len   = 0;
#endif /* TRIO_MMIO */

  fix->type = FB_TYPE_PACKED_PIXELS;
  fix->type_aux = 0;
  if(par->var.bits_per_pixel == 15 || par->var.bits_per_pixel == 16) {
    fix->visual = FB_VISUAL_DIRECTCOLOR;
  } else {
    fix->visual = FB_VISUAL_PSEUDOCOLOR;
  }

  fix->xpanstep  = 0;
  fix->ypanstep  = 0;
  fix->ywrapstep = 0;
  fix->line_length = 
    triofb_default.xres*(triofb_default.bits_per_pixel ==  8 ? 1 : 
			 triofb_default.bits_per_pixel == 15 ? 2 : 2);
  fix->accel = FB_ACCEL_S3_TRIO64;
  
  return(0);
}

static int Trio_decode_var(struct fb_var_screeninfo *var,
			   struct triofb_par *par) {
  if(var->bits_per_pixel !=  8 &&
     var->bits_per_pixel != 15 &&
     var->bits_per_pixel != 16)
    return 1;

  par->var.xres = var->xres;
  par->var.yres = var->yres;
  par->var.xres_virtual = var->xres_virtual;
  par->var.yres_virtual = var->yres_virtual;
  par->var.xoffset = var->xoffset;
  par->var.yoffset = var->yoffset;
  par->var.bits_per_pixel = var->bits_per_pixel;
  par->var.grayscale = var->grayscale;

  switch(var->bits_per_pixel) {
  case 8:
    par->var.red.offset      = 0;
    par->var.red.length      = 8;
    par->var.red.msb_right   = 0;

    par->var.green.offset    = 0;
    par->var.green.length    = 8;
    par->var.green.msb_right = 0;

    par->var.blue.offset     = 0;
    par->var.blue.length     = 8;
    par->var.blue.msb_right  = 0;
    break;

  case 15:
    par->var.red.offset      = 10;
    par->var.red.length      = 5;
    par->var.red.msb_right   = 0;

    par->var.green.offset    = 5;
    par->var.green.length    = 5;
    par->var.green.msb_right = 0;

    par->var.blue.offset     = 0;
    par->var.blue.length     = 5;
    par->var.blue.msb_right  = 0;
    break;

  case 16:
    par->var.red.offset      = 11;
    par->var.red.length      = 5;
    par->var.red.msb_right   = 0;

    par->var.green.offset    = 5;
    par->var.green.length    = 6;
    par->var.green.msb_right = 0;

    par->var.blue.offset     = 0;
    par->var.blue.length     = 5;
    par->var.blue.msb_right  = 0;
    break;
  }

  par->var.transp.offset    = 0;
  par->var.transp.length    = 8;
  par->var.transp.msb_right = 0;

  par->var.nonstd = var->nonstd;
  par->var.activate = var->activate;
  par->var.height = var->height;
  par->var.width = var->width;
  if (var->accel_flags & FB_ACCELF_TEXT) {
    par->var.accel_flags = FB_ACCELF_TEXT;
  } else {
    par->var.accel_flags = 0;
  }
  par->var.pixclock = var->pixclock;
  par->var.left_margin = var->left_margin;
  par->var.right_margin = var->right_margin;
  par->var.upper_margin = var->upper_margin;
  par->var.lower_margin = var->lower_margin;
  par->var.hsync_len = var->hsync_len;
  par->var.vsync_len = var->vsync_len;
  par->var.sync = var->sync;
  par->var.vmode = var->vmode;
  return(0);
}

static int Trio_encode_var(struct fb_var_screeninfo *var,
			   struct triofb_par *par) {
  var->xres = par->var.xres;
  var->yres = par->var.yres;
  var->xres_virtual = par->var.xres_virtual;
  var->yres_virtual = par->var.yres_virtual;
  var->xoffset = par->var.xoffset;
  var->yoffset = par->var.yoffset;
  
  var->bits_per_pixel = par->var.bits_per_pixel;
  var->grayscale = par->var.grayscale;
  
  var->red = par->var.red;
  var->green = par->var.green;
  var->blue = par->var.blue;
  var->transp = par->var.transp;
  
  var->nonstd = par->var.nonstd;
  var->activate = par->var.activate;
  
  var->height = par->var.height;
  var->width = par->var.width;
  
  var->accel_flags = par->var.accel_flags;
  
  var->pixclock = par->var.pixclock;
  var->left_margin = par->var.left_margin;
  var->right_margin = par->var.right_margin;
  var->upper_margin = par->var.upper_margin;
  var->lower_margin = par->var.lower_margin;
  var->hsync_len = par->var.hsync_len;
  var->vsync_len = par->var.vsync_len;
  var->sync = par->var.sync;
  var->vmode = par->var.vmode;
  
  return(0);
}

static int Trio_setcolreg(dword regno, 
			  dword red, 
			  dword green, 
			  dword blue,
			  dword transp, 
			  struct fb_info *info) {
  if (regno > 255) { return (1); }
  
  vga_outb(0x3c8, (unsigned char) regno);
  
  red   >>= 10;
  green >>= 10;
  blue  >>= 10;
  
  Trio_colour_table [regno][0] = red;
  Trio_colour_table [regno][1] = green;
  Trio_colour_table [regno][2] = blue;
  
  vga_outb(0x3c9, red);
  vga_outb(0x3c9, green);
  vga_outb(0x3c9, blue);
  
  return (0);
}

static int Trio_getcolreg(dword regno, 
			   dword *red, 
			   dword *green, 
			   dword *blue,
			   dword *transp, 
			   struct fb_info *info) {
  int t;
  
  if (regno > 255) {
    return (1);
  }
  /* ARB This shifting & oring seems VERY strange */
  t	= Trio_colour_table [regno][0];
  *red	= (t<<10) | (t<<4) | (t>>2);
  t	= Trio_colour_table [regno][1];
  *green	= (t<<10) | (t<<4) | (t>>2);
  t	= Trio_colour_table [regno][2];
  *blue	= (t<<10) | (t<<4) | (t>>2);
  *transp = 0;
  return (0);
}

void Trio_blank(int blank) {
  int i;
  
  if (blank) {
    for (i = 0; i < 256; i++) {
      vga_outb(0x3c8, (unsigned char) i);
      vga_outb(0x3c9, 0); 
      vga_outb(0x3c9, 0);
      vga_outb(0x3c9, 0);
    }
  } else {
    for (i = 0; i < 256; i++) {
      vga_outb(0x3c8, (unsigned char) i);
      vga_outb(0x3c9, Trio_colour_table[i][0]);
      vga_outb(0x3c9, Trio_colour_table[i][1]);
      vga_outb(0x3c9, Trio_colour_table[i][2]);
    }
  }
}

static void Trio_MoveCursor (word x, 
			      word y) {
  
  crt_outb(CRT_ID_HWGC_ORIGIN_X_HI, (char)((x & 0x0700) >> 8));
  crt_outb(CRT_ID_HWGC_ORIGIN_X_LO, (char)(x & 0x00ff));
  crt_outb(CRT_ID_HWGC_ORIGIN_Y_HI, (char)((y & 0x0700) >> 8));
  crt_outb(CRT_ID_HWGC_ORIGIN_Y_LO, (char)(y & 0x00ff));
}

static struct fb_hwswitch Trio_switch = {
  Trio_init, Trio_encode_fix, Trio_decode_var, Trio_encode_var,
  Trio_getcolreg, Trio_setcolreg, Trio_blank
};

static void triofb_get_par(struct triofb_par *par) {
  if (current_par_valid) {
    *par = current_par;
  } else {
    fbhw->decode_var(&triofb_default, par);
  }
}

static void triofb_set_par(struct triofb_par *par) {
  current_par = *par;
  current_par_valid = 1;
}

static void trio_set_video(struct fb_var_screeninfo *var) {
  /* Load the video mode defined by the 'var' data */
  trio_load_video_mode (var);
}

static int do_fb_set_var(struct fb_var_screeninfo *var, 
			 int isactive) {
  int err, activate;
  struct triofb_par par;
  
  if ((err = fbhw->decode_var(var, &par))) {
    return(err);
  }
  activate = var->activate;
  if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW && isactive)
    triofb_set_par(&par);
  fbhw->encode_var(var, &par);
  var->activate = activate;
  
  trio_set_video(var);
  return 0;
}

static void do_install_cmap(int con, struct fb_info *info) {
  if (con != currcon) {
    return;
  }
  /*if (fb_display[con].cmap.len) {
    fb_set_cmap(&fb_display[con].cmap, 1, fbhw->setcolreg, info);
  } else {
    fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
		1, fbhw->setcolreg, info);
  }*/
}

static int triofb_open(struct fb_info *info, int user) {
  //MOD_INC_USE_COUNT;
  return(0);
}

static int triofb_release(struct fb_info *info, int user) {
  //MOD_DEC_USE_COUNT;
  return(0);
}

static int triofb_get_fix(struct fb_fix_screeninfo *fix, 
			   int con,
			   struct fb_info *info) {
  struct triofb_par par;
  int error = 0;
  
  if (con == -1) {
    triofb_get_par(&par);
  } else {
    //error = fbhw->decode_var(&fb_display[con].var, &par);
  }
  return(error ? error : fbhw->encode_fix(fix, &par));
}

static int triofb_get_var(struct fb_var_screeninfo *var, 
			   int con,
			   struct fb_info *info) {
  struct triofb_par par;
  int error = 0;
  
  if (con == -1) {
    triofb_get_par(&par);
    error = fbhw->encode_var(var, &par);
    //disp.var = *var;   /* ++Andre: don't know if this is the right place */
  } else {
    //*var = fb_display[con].var;
  }
  
  return(error);
}

static void triofb_set_disp(int con, 
			     struct fb_info *info) {
#if 0
  struct fb_fix_screeninfo fix;
  struct display *display;

  if (con >= 0)
    display = &fb_display[con];
  else
    display = &disp;	/* used during initialization */
  
  triofb_get_fix(&fix, con, info);
  if (con == -1)
    con = 0;
  display->screen_base    = (char*)TrioMem;
  display->visual         = fix.visual;
  display->type           = fix.type;
  display->type_aux       = fix.type_aux;
  display->ypanstep       = fix.ypanstep;
  display->ywrapstep      = fix.ywrapstep;
  display->can_soft_blank = 1;
  display->inverse        = Triofb_inverse;
  switch (display->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
  case 8:
    display->dispsw = &fbcon_cfb8;
#ifdef TRIO_ACCEL
    if(display->var.accel_flags & FB_ACCELF_TEXT) 
      display->dispsw = &fbcon_trio8;
#endif /* TRIO_ACCEL */
    break;
#endif /* FBCON_HAS_CFB8 */
#ifdef FBCON_HAS_CFB16
  case 16:
    display->dispsw = &fbcon_cfb16;
    break;
#endif /* FBCON_HAS_CFB16 */
  default:
    display->dispsw = NULL;
    break;
  }
#endif
}

static int triofb_set_var(struct fb_var_screeninfo *var, 
			   int con,
			   struct fb_info *info) {
  int err;//, oldxres, oldyres, oldvxres, oldvyres, oldbpp, oldaccel;
  
  if ((err = do_fb_set_var(var, con == currcon))) {
    return(err);
  }
  if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
    /*oldxres = fb_display[con].var.xres;
    oldyres = fb_display[con].var.yres;
    oldvxres = fb_display[con].var.xres_virtual;
    oldvyres = fb_display[con].var.yres_virtual;
    oldbpp = fb_display[con].var.bits_per_pixel;
    oldaccel = fb_display[con].var.accel_flags;
    fb_display[con].var = *var;
    if (oldxres != var->xres || oldyres != var->yres ||
	oldvxres != var->xres_virtual ||
	oldvyres != var->yres_virtual ||
	oldbpp != var->bits_per_pixel ||
	oldaccel != var->accel_flags) {
      triofb_set_disp(con, info);
      (*fb_info.changevar)(con);
      fb_alloc_cmap(&fb_display[con].cmap, 0, 0);
      do_install_cmap(con, info);
    }*/
  }
  var->activate = 0;
  return(0);
}

static int triofb_get_cmap(struct fb_cmap *cmap, 
			    int kspc, 
			    int con,
			    struct fb_info *info) {
#if 0
  if (con == currcon) { /* current console? */
    return(fb_get_cmap(cmap, kspc, fbhw->getcolreg, info));
  } else if (fb_display[con].cmap.len) { /* non default colormap? */
    fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
  } else {
    fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
		 cmap, kspc ? 0 : 2);
  }
#endif
  return(0);
}

static int triofb_set_cmap(struct fb_cmap *cmap, 
			    int kspc, 
			    int con,
			    struct fb_info *info) {
#if 0
  int err;
  
  if (!fb_display[con].cmap.len) {       /* no colormap allocated? */
    if ((err = fb_alloc_cmap(&fb_display[con].cmap,
			     1<<fb_display[con].var.bits_per_pixel,
			     0))) {
      return(err);
    }
  }
  if (con == currcon) {		 /* current console? */
    return(fb_set_cmap(cmap, kspc, fbhw->setcolreg, info));
  } else {
    fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
  }
#endif
  return(0);
}

static int triofb_pan_display(struct fb_var_screeninfo *var, 
			       int con,
			       struct fb_info *info) {
  return 1;
}

static int triofb_ioctl(struct inode *inode, 
			 struct file *file,
			 dword cmd, 
			 dword arg, 
			 int con, 
			 struct fb_info *info) {
  return(1);
}


static struct 
{
	int (*open)(struct fb_info *info, int user);
	int (*release)(struct fb_info *info, int user);
	int (*get_fix)(struct fb_fix_screeninfo *fix, int con,
			   struct fb_info *info);
	int (*get_var)(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info);
	int (*set_var)(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info);
	int (*get_cmap)(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info);
	int (*set_cmap)(struct fb_cmap *cmap, int kspc, int con,
			    struct fb_info *info);
	int (*pan_display)(struct fb_var_screeninfo *var, int con,
			       struct fb_info *info);
	int (*ioctl)(struct inode *inode, struct file *file, dword cmd,
			 dword arg, int con, struct fb_info *info);
} triofb_ops =
{
  triofb_open, triofb_release, triofb_get_fix, triofb_get_var,
  triofb_set_var, triofb_get_cmap, triofb_set_cmap,
  triofb_pan_display, triofb_ioctl
};

/*void trio_setup(char *options, int *ints) {
  char *this_opt;
  
  fb_info.fontname[0] = '\0';
  
  if (!options || !*options) {
    return;
  }
  
  for (this_opt = strtok(options, ","); this_opt;
       this_opt = strtok(NULL, ",")) {
    if (!strcmp(this_opt, "inverse")) {
      Triofb_inverse = 1;
      fb_invert_cmaps();
    } else if (!strncmp(this_opt, "font:", 5)) {
      strcpy(fb_info.fontname, this_opt+5);
    } else if (!strcmp (this_opt, "trio8")) {
      triofb_default = triofb_predefined[TRIO8_DEFMODE].var;
    } else if (!strcmp (this_opt, "trio16")) {
      triofb_default = triofb_predefined[TRIO16_DEFMODE].var;
    } else get_video_mode(this_opt);
  }
  
}*/

void trio_init(void) {
  /*struct pci_dev *pdev;
  
  for(pdev = pci_devices; pdev; pdev = pdev->next) {
    if(((pdev->class >> 16) == PCI_BASE_CLASS_DISPLAY) &&
       (pdev->vendor == PCI_VENDOR_ID_S3) &&
       (pdev->device == PCI_DEVICE_ID_S3_TRIO)) {*/
      
      struct triofb_par par;
      unsigned long board_addr;
      unsigned long board_size;
      unsigned char tmp;
      pci_cfg_t cfg;
      
	  if (!pci_find(PCI_VENDOR_ID_S3, PCI_DEVICE_ID_S3_TRIO, &cfg))
      {
		  wprintf(L"No S3Trio device found\n");
		  return;
	  }

      /* goto color emulation, enable CPU access */
      vga_outb(GREG_MISC_OUTPUT_W, vga_inb(GREG_MISC_OUTPUT_R) | 0x03);
      
      /* unlock registers */
      crt_outb(CRT_ID_REGISTER_LOCK_1, 0x48);
      crt_outb(CRT_ID_REGISTER_LOCK_2, 0xa5);
      crt_outb(CRT_ID_SYSTEM_CONFIG, crt_inb(CRT_ID_SYSTEM_CONFIG) | 0x01);
      crt_outb(CRT_ID_REGISTER_LOCK, crt_inb(CRT_ID_REGISTER_LOCK) & ~0x30);
      seq_outb(SEQ_ID_UNLOCK_EXT, 0x06);
      crt_outb(CRT_ID_BACKWAD_COMP_2, 
	       (crt_inb(CRT_ID_BACKWAD_COMP_2) & 
		~(0x2 | 0x10 |  0x40)) | 0x20);
      
      /* get Trio identification, 0x10 = trio32, 0x11 = trio64 */
      if(crt_inb(CRT_ID_DEVICE_LOW) != 0x11)
	  {
		  wprintf(L"Not a Trio64 device\n");
		  return;
	  }
      
      tmp = crt_inb(CRT_ID_CONFIG_1);
      board_size = 
	(tmp & 0xe0) == 0x00 ? 4096*1024 :
	(tmp & 0xe0) == 0x80 ? 2048*1024 :
	(tmp & 0xe0) == 0xc0 ? 1024*1024 : 512*1024;
	  //board_size = 65536;
      
      //board_addr = pdev->base_address[0] & PCI_BASE_ADDRESS_MEM_MASK;
	  board_addr = cfg.base[0] & PCI_BASE_ADDRESS_MEM_MASK;
	  
      //TrioMem       = (unsigned long)ioremap_nocache(board_addr, 
						     //64*1024*1024);
	  TrioMem       = board_addr;
      TrioMem_phys  = board_addr;
      TrioSize      = board_size;
#ifdef TRIO_MMIO
      TrioRegs      = 0xa0000;
#endif
      fbhw = &Trio_switch;
      
      /*strcpy(fb_info.modename, triofb_name);
      fb_info.changevar = NULL;
      fb_info.node = -1;
      fb_info.fbops = &triofb_ops;
      fb_info.disp = &disp;
      fb_info.switch_con = &Triofb_switch;
      fb_info.updatevar = &Triofb_updatevar;
      fb_info.blank = &Triofb_blank;*/
      
      fbhw->init();
      fbhw->decode_var(&triofb_default, &par);
      fbhw->encode_var(&triofb_default, &par);
      
      do_fb_set_var(&triofb_default, 1);

      //triofb_get_var(&fb_display[0].var, -1, &fb_info);
      //triofb_set_disp(-1, &fb_info);
      //do_install_cmap(0, &fb_info);
      
      /*if (register_framebuffer(&fb_info) < 0) {
	return;
      }*/
      
      wprintf(L"frame buffer device, using %ldK of video memory\n",
	     /*GET_FB_IDX(fb_info.node), fb_info.modename, */TrioSize>>10);
      
      /* TODO: This driver cannot be unloaded yet */
      //MOD_INC_USE_COUNT;
    }
  //}
//}


static int Triofb_switch(int con, struct fb_info *info) {
#if 0
  /* Do we have to save the colormap? */
  if (fb_display[currcon].cmap.len) {
    fb_get_cmap(&fb_display[currcon].cmap, 1, fbhw->getcolreg,
		info);
  }
  
  do_fb_set_var(&fb_display[con].var, 1);
  currcon = con;
  /* Install new colormap */
  do_install_cmap(con, info);
#endif
  return(0);
}

static int Triofb_updatevar(int con, 
			    struct fb_info *info) {
  return(0);
}

static void Triofb_blank(int blank, struct fb_info *info) {
  fbhw->blank(blank);
}

static int get_video_mode(const char *name) {
  int i;
  
  for (i = 0; i < NUM_TOTAL_MODES; i++) {
    if (!strcmp(name, triofb_predefined[i].name)) {
      triofb_default = triofb_predefined[i].var;
      return(i);
    }
  }
  /* ++Andre: set triofb default mode */
  triofb_default = triofb_predefined[TRIO8_DEFMODE].var;
  return(0);
}

#ifdef TRIO_ACCEL
static void Trio_WaitQueue(void) {
  word status;
  do { status = trio_inw(S3_GP_STAT); }  while(!(status & S3_FIFO_EMPTY));
}

static void Trio_WaitBlit (void) {
  word status;
  do { status = trio_inw(S3_GP_STAT); } while (status & S3_HDW_BUSY);
}

static void Trio_WaitIdle (void) {
  Trio_WaitQueue();
  Trio_WaitBlit();
}

static void Trio_BitBLT (word curx, 
			  word cury, 
			  word destx,
			  word desty, 
			  word width, 
			  word height,
			  word mode) {
  word blitcmd = S3_BITBLT;
  
  /* Set drawing direction */
  /* -Y, X maj, -X (default) */
  if (curx > destx) {
    blitcmd |= 0x0020;  /* Drawing direction +X */
  } else {
    curx  += (width - 1);
    destx += (width - 1);
  }
  
  if (cury > desty) {
    blitcmd |= 0x0080;  /* Drawing direction +Y */
  } else {
    cury  += (height - 1);
    desty += (height - 1);
  }
  
  Trio_WaitIdle();
  trio_outw(S3_PIXEL_CNTL,    0xa000);
  trio_outw(S3_FRGD_MIX,      0x0060 | mode);

  Trio_WaitIdle();  
  trio_outw(S3_CUR_X,         curx);
  trio_outw(S3_CUR_Y,         cury);
  trio_outw(S3_DESTX_DIA,     destx);
  trio_outw(S3_DESTY_AX,      desty);
  trio_outw(S3_MIN_AXIS_PCNT, height - 1);
  trio_outw(S3_MAJ_AXIS_PCNT, width - 1);

  Trio_WaitIdle();  
  trio_outw(S3_CMD, blitcmd);
  
}

static void Trio_RectFill (word x, 
			    word y, 
			    word width,
			    word height, 
			    word color) {
  Trio_WaitIdle();  
  trio_outw(S3_PIXEL_CNTL, 0xa000);
  trio_outw(S3_FRGD_COLOR, color);
  trio_outw(S3_FRGD_MIX,   0x0027);
  trio_outw(S3_WRT_MASK,   0xffff);

  Trio_WaitIdle();
  trio_outw(S3_CUR_X, x & 0x0fff);
  trio_outw(S3_CUR_Y, y & 0x0fff);
  trio_outw(S3_MIN_AXIS_PCNT, (height - 1) & 0x0fff);
  trio_outw(S3_MAJ_AXIS_PCNT, (width  - 1) & 0x0fff);
  trio_outw(S3_CMD, 0x40b1);
}

#ifdef FBCON_HAS_CFB8
static void fbcon_trio8_bmove(struct display *p, int sy, int sx, int dy,
			       int dx, int height, int width) {
  sx *= 8; dx *= 8; width *= 8;
  Trio_BitBLT((word)sx, (word)(sy*fontheight(p)), (word)dx,
	      (word)(dy*fontheight(p)), (word)width,
	      (word)(height*fontheight(p)), (word)S3_NEW);
}

static void fbcon_trio8_clear(struct vc_data *conp, struct display *p, int sy,
			       int sx, int height, int width) {
  unsigned char bg;
  
  sx *= 8; width *= 8;
  bg = attr_bgcol_ec(p,conp);
  Trio_RectFill((word)sx,
		(word)(sy*fontheight(p)),
		(word)width,
		(word)(height*fontheight(p)),
		(word)bg);
}

static void fbcon_trio8_putc(struct vc_data *conp, struct display *p, int c,
			      int yy, int xx) {
  Trio_WaitBlit();
  fbcon_cfb8_putc(conp, p, c, yy, xx);
}

static void fbcon_trio8_putcs(struct vc_data *conp, struct display *p,
			       const unsigned short *s, int count,
			       int yy, int xx) {
  Trio_WaitBlit();
  fbcon_cfb8_putcs(conp, p, s, count, yy, xx);
}

static void fbcon_trio8_revc(struct display *p, int xx, int yy) {
  Trio_WaitBlit();
  fbcon_cfb8_revc(p, xx, yy);
}

static struct display_switch fbcon_trio8 = {
   fbcon_cfb8_setup, fbcon_trio8_bmove, fbcon_trio8_clear, fbcon_trio8_putc,
   fbcon_trio8_putcs, fbcon_trio8_revc, NULL, NULL, fbcon_cfb8_clear_margins,
   FONTWIDTH(8)
};
#endif /* FBCON_HAS_CFB8 */
#endif /* TRIO_ACCEL */

#ifdef MODULE
int init_module(void) {
  triofb_init();
  return 0;
}

void cleanup_module(void) {
  /* Not reached because the usecount will never be
     decremented to zero */
  unregister_framebuffer(&fb_info);
  /* TODO: clean up ... */
}
#endif /* MODULE */

#define MAXPIXELCLOCK 135000000 /* safety */

/* Console colors */
unsigned char cvconscolors[16][3] = {	/* background, foreground, hilite */
  /*  R     G     B  */
  {0x30, 0x30, 0x30},
  {0x00, 0x00, 0x00},
  {0x80, 0x00, 0x00},
  {0x00, 0x80, 0x00},
  {0x00, 0x00, 0x80},
  {0x80, 0x80, 0x00},
  {0x00, 0x80, 0x80},
  {0x80, 0x00, 0x80},
  {0xff, 0xff, 0xff},
  {0x40, 0x40, 0x40},
  {0xff, 0x00, 0x00},
  {0x00, 0xff, 0x00},
  {0x00, 0x00, 0xff},
  {0xff, 0xff, 0x00},
  {0x00, 0xff, 0xff},
  {0x00, 0x00, 0xff}
};

inline void trio_video_disable(int toggle) {
  int r;
  
  toggle &= 0x1;
  toggle = toggle << 5;
  
  r = (int) seq_inb(SEQ_ID_CLOCKING_MODE);
  r &= 0xdf;	/* Set bit 5 to 0 */
  
  seq_outb(SEQ_ID_CLOCKING_MODE, r | toggle);
}

/*
 * Computes M, N, and R values from
 * given input frequency. It uses a table of
 * precomputed values, to keep CPU time low.
 *
 * The return value consist of:
 * lower byte:  Bits 4-0: N Divider Value
 *	        Bits 5-6: R Value          for e.g. SR10 or SR12
 * higher byte: Bits 0-6: M divider value  for e.g. SR11 or SR13
 */
static unsigned short trio_compute_clock(unsigned long freq) {
  /*
   * Calculate the video clocks;
   *
   * Fout = Fref * (M + 2) / ((N + 2) * 2^R)
   * 
   *      where 0 <= R <= 3, 1 <= N <= 31, 1 <= M <= 127, and
   *      135 MHz <= ((M + 2)*Fref) / (N + 2) <= 270 MHz
   *      
   */
    
  double Fref = 14.31818;
  double Fout = ((double)freq) / 1.0e6;
  dword N, M, R;
  word mnr;

do_clocks:

  for(R = 0; R < 4; R++)
    if(((double)(1 << R))*Fout >  135.0 &&
       ((double)(1 << R))*Fout <= 270.0)
      break;

  if(R == 4) {
    wprintf(L"TRIO driver: Unsupported clock freq %ld, using 25MHz\n", 
           (long)(Fout*1.0e6));
    Fout = 25.000;
    goto do_clocks;
  }

  for(N = 1; N < 32; N++) {
    double Ftry;
    M = (int)(Fout*((double)(N + 2))*((double)(1 << R))/Fref - 2.0);
    Ftry = ((double)(M + 2))*Fref/(((double)(N + 2))*((double)(1 << R)));
    if(0.995*Fout < Ftry && 1.005*Fout > Ftry)
      break;
  }
  
  if(N == 32) {
    wprintf(L"TRIO driver: Unsupported clock freq %ld, using 25MHz\n", 
           (long)(Fout*1.0e6));
    Fout = 25.000;
    goto do_clocks;
  }
  
  mnr = ((M & 0x7f) << 8) | ((R & 0x03) << 5) | (N & 0x1f);
  return mnr;
}

static void trio_load_video_mode (struct fb_var_screeninfo *video_mode) {
  int fx, fy;
  unsigned short mnr;
  unsigned char HT, HDE, HBS, HBE, HSS, HSE, VDE, VBS, VBE, VSS, VSE, VT;
  unsigned char FIFO;
  int cr50, sr15, sr18, clock_mode, test;
  int m, n;
  int tfillm, temptym;
  int hmul;

  /* ---------------- */
  int xres, hfront, hsync, hback;
  int yres, vfront, vsync, vback;
  int bpp;
  long freq;
  /* ---------------- */
	
  fx = fy = 8;	/* force 8x8 font */

/* GRF - Disable interrupts */	
	
  switch (video_mode->bits_per_pixel) {
  case 15:
  case 16:
    hmul = 2;
    break;                
  default:
    hmul = 1;
    break;
  }
        
  trio_video_disable(1);
	
  bpp    = video_mode->bits_per_pixel;
  xres   = video_mode->xres;
  hfront = video_mode->right_margin;
  hsync  = video_mode->hsync_len;
  hback  = video_mode->left_margin;

  yres   = video_mode->yres;
  vfront = video_mode->lower_margin;
  vsync  = video_mode->vsync_len;
  vback  = video_mode->upper_margin;

  HBS = HDE = hmul*(xres)/8 - 1;
  HSS =       hmul*(xres + hfront)/8 - 1;
  HSE =       hmul*(xres + hfront + hsync)/8 - 1;
  HBE = HT  = hmul*(xres + hfront + hsync + hback)/8 - 1;
  HBE = HSE; /* weirdness appeared if HBE = HT */

  FIFO = (HBS + (HBE - 5))/2;

  VBS = VDE = yres - 1;
  VSS =       yres + vfront - 1;
  VSE =       yres + vfront + vsync - 1;
  VBE = VT  = yres + vfront + vsync + vback - 2;

  crt_outb(0x11, crt_inb(0x11) & 0x7f); /* unlock crt 0-7 */

  crt_outb(CRT_ID_HWGC_MODE,    0x00);
  crt_outb(CRT_ID_EXT_DAC_CNTL, 0x00);
	
  /* sequential addressing, chain-4 */
  seq_outb(SEQ_ID_MEMORY_MODE, 0x0e);

  gra_outb(GCT_ID_READ_MAP_SELECT, 0x00);
  seq_outb(SEQ_ID_MAP_MASK,        0xff);
  seq_outb(SEQ_ID_CHAR_MAP_SELECT, 0x00);
	
  /* trio_compute_clock accepts arguments in Hz */
  /* pixclock is in ps ... convert to Hz */
	
  freq = (1000000000 / video_mode->pixclock) * 1000;

  mnr = trio_compute_clock (freq);
  seq_outb(SEQ_ID_DCLK_HI, ((mnr & 0xFF00) >> 8));
  seq_outb(SEQ_ID_DCLK_LO, (mnr & 0xFF));
	
  crt_outb(CRT_ID_MEMORY_CONF,    0x08); 
  crt_outb(CRT_ID_BACKWAD_COMP_1, 0x00); /* - */
  crt_outb(CRT_ID_REGISTER_LOCK,  0x00);
  crt_outb(CRT_ID_EXT_MODE,       0x00);

  /* Load display parameters into board */
  crt_outb(CRT_ID_EXT_HOR_OVF,
       (((HT - 4) & 0x100) ? 0x01 : 0x00)  |
       ((HDE & 0x100) ? 0x02 : 0x00) |
       ((HBS & 0x100) ? 0x04 : 0x00) |
       /* ((HBE & 0x40) ? 0x08 : 0x00)  | */
       ((HSS & 0x100) ? 0x10 : 0x00) |
       /* ((HSE & 0x20) ? 0x20 : 0x00)  | */
       ((FIFO & 0x100) ? 0x40 : 0x00)
       );
	
  crt_outb(CRT_ID_EXT_VER_OVF,
       0x40 |
       ((VT & 0x400) ? 0x01 : 0x00) |
       ((VDE & 0x400) ? 0x02 : 0x00) |
       ((VBS & 0x400) ? 0x04 : 0x00) |
       ((VSS & 0x400) ? 0x10 : 0x00)
       );
  
  crt_outb(CRT_ID_HOR_TOTAL, HT - 4);
  crt_outb(CRT_ID_DISPLAY_FIFO, FIFO);
  crt_outb(CRT_ID_HOR_DISP_ENA_END, HDE);
  crt_outb(CRT_ID_START_HOR_BLANK, HBS);
  crt_outb(CRT_ID_END_HOR_BLANK, (HBE & 0x1F));
  crt_outb(CRT_ID_START_HOR_RETR, HSS);
  crt_outb(CRT_ID_END_HOR_RETR,
       (HSE & 0x1F) |
       ((HBE & 0x20) ? 0x80 : 0x00)
       );
  crt_outb(CRT_ID_VER_TOTAL, VT);
  crt_outb(CRT_ID_OVERFLOW,
       0x10 |
       ((VT & 0x100) ? 0x01 : 0x00)  |
       ((VDE & 0x100) ? 0x02 : 0x00) |
       ((VSS & 0x100) ? 0x04 : 0x00) |
       ((VBS & 0x100) ? 0x08 : 0x00) |
       ((VT & 0x200) ? 0x20 : 0x00)  |
       ((VDE & 0x200) ? 0x40 : 0x00) |
       ((VSS & 0x200) ? 0x80 : 0x00)
       );
  crt_outb(CRT_ID_MAX_SCAN_LINE,
       0x40 |
       ((VBS & 0x200) ? 0x20 : 0x00)
       );
	
  crt_outb(CRT_ID_MODE_CONTROL, 0xe3); /* . */

  crt_outb(CRT_ID_UNDERLINE_LOC, 0x00);

  /* start address */
  crt_outb(CRT_ID_EXT_SYS_CNTL_3,  0x00);
  crt_outb(CRT_ID_START_ADDR_HIGH, 0x00);
  crt_outb(CRT_ID_START_ADDR_LOW,  0x00);

  crt_outb(CRT_ID_START_VER_RETR, VSS);
  crt_outb(CRT_ID_END_VER_RETR, (VSE & 0x0F));
  crt_outb(CRT_ID_VER_DISP_ENA_END, VDE);
  crt_outb(CRT_ID_START_VER_BLANK, VBS);
  crt_outb(CRT_ID_END_VER_BLANK, VBE);
  crt_outb(CRT_ID_LINE_COMPARE, 0xFF);
  crt_outb(CRT_ID_LACE_RETR_START, HT / 2);
  crt_outb(CRT_ID_LACE_CONTROL, 0x00);
  gra_outb(GCT_ID_GRAPHICS_MODE, 0x40);
  gra_outb(GCT_ID_MISC, 0x01);
  seq_outb(SEQ_ID_MEMORY_MODE, 0x02);
	
  vga_outb(VDAC_MASK, 0xFF);
	
  /* Blank border */
  test = crt_inb(CRT_ID_BACKWAD_COMP_2);
  crt_outb(CRT_ID_BACKWAD_COMP_2, test | 0x20); /* - */
	
  sr15 = seq_inb(SEQ_ID_CLKSYN_CNTL_2);
  sr15 &= 0xEF;
  sr18 = seq_inb(SEQ_ID_RAMDAC_CNTL);
  sr18 &= 0x7F;
  clock_mode = 0x00;
  cr50 = 0x00;
	
  test = crt_inb(CRT_ID_EXT_MISC_CNTL_2);
  test &= 0xD;
	
  switch (video_mode->bits_per_pixel) {
  case 8:
    if (freq > 80000000) {
      clock_mode = 0x10 | 0x02;
      sr15 |= 0x10;
      sr18 |= 0x80;
    }
    HDE = video_mode->xres/8;
    cr50 |= 0x00;
    break;
  case 15:
    clock_mode = 0x30;
    HDE = video_mode->xres / 4;
    cr50 |= 0x10;
    break;            
  case 16:
    clock_mode = 0x50;
    HDE = video_mode->xres / 4;
    cr50 |= 0x10;
    break;
  }
	
  crt_outb(CRT_ID_EXT_MISC_CNTL_2, clock_mode | test);
  seq_outb(SEQ_ID_CLKSYN_CNTL_2, sr15);
  seq_outb(SEQ_ID_RAMDAC_CNTL, sr18);
  crt_outb(CRT_ID_SCREEN_OFFSET, HDE);

  crt_outb(CRT_ID_MISC_1, 0xb5); 
	
  test = (HDE >> 4) & 0x30;
  crt_outb(CRT_ID_EXT_SYS_CNTL_2, test);
	
  /* Set up graphics engine */
  switch (video_mode->xres) {
  case 1024:
    cr50 |= 0x00;
    break;
		
  case 640:
    cr50 |= 0x40;
    break;
		
  case 800:
    cr50 |= 0x80;
    break;
		
  case 1280:
    cr50 |= 0xC0;
    break;
		
  case 1152:
    cr50 |= 0x01;
    break;
		
  case 1600:
    cr50 |= 0x81;
    break;
		
  default:	/* XXX */
    break;
  }
	
  crt_outb(CRT_ID_EXT_SYS_CNTL_1, cr50);
	
  att_outb(ACT_ID_ATTR_MODE_CNTL, 0x41);
  att_outb(ACT_ID_COLOR_PLANE_ENA, 0x0f);
	
  tfillm = (96 * (trio_memclk / 1000)) / 240000;
	
  switch (video_mode->bits_per_pixel) {
  case 15:
  case 16:
    temptym = (48 * (trio_memclk / 1000)) / (freq / 1000);
    break;
  default:
    temptym = (96 * (trio_memclk / 1000)) / (freq / 1000);
    break;
  }
	
  m = (temptym - tfillm - 9) / 2;
  if (m < 0)
    m = 0;
  m = (m & 0x1F) << 3;
  if (m < 0x18)
    m = 0x18;
  n = 0xFF;
	
  crt_outb(CRT_ID_EXT_MEM_CNTL_2, m);
  crt_outb(CRT_ID_EXT_MEM_CNTL_3, n);
	
  att_outb(0x33, 0);
	
  /* Turn gfx on again */
  trio_video_disable(0);
	
#ifdef TRIO_ACCEL
  Trio_WaitIdle();
  trio_outw(S3_FRGD_MIX, 0x0007);
  trio_outw(S3_WRT_MASK, 0x0027);

  Trio_WaitIdle();
  trio_outw(S3_READ_REG_DATA, 0x1000); /* set clip rect */
  trio_outw(S3_READ_REG_DATA, 0x2000);
  trio_outw(S3_READ_REG_DATA, 0x3fff);
  trio_outw(S3_READ_REG_DATA, 0x4fff);

  Trio_WaitIdle();
  trio_outw(S3_RD_MASK, 0xffff);  /* set masks */
  trio_outw(S3_WRT_MASK, 0xffff);

#endif /* TRIO_ACCEL */
}

