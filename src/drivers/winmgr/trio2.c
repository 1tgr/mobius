/*
*  linux/drivers/video/S3Triofb.c -- Open Firmware based frame buffer device
*
*	   Copyright (C) 1997 Peter De Schrijver
*
*  This driver is partly based on the PowerMac console driver:
*
*	   Copyright (C) 1996 Paul Mackerras
*
*  and on the Open Firmware based frame buffer device:
*
*	   Copyright (C) 1997 Geert Uytterhoeven
*
*  This file is subject to the terms and conditions of the GNU General Public
*  License. See the file COPYING in the main directory of this archive for
*  more details.
*/

/*
Bugs : + OF dependencies should be removed.
+ This driver should be merged with the CyberVision driver. The
CyberVision is a Zorro III implementation of the S3Trio64 chip.

*/

/*#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/selection.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <linux/pci.h>
#ifdef CONFIG_FB_COMPAT_XPMAC
#include <asm/vc_ioctl.h>
#endif

  #include <video/fbcon.h>
  #include <video/fbcon-cfb8.h>
#include <video/s3blit.h>*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fb.h"
#include "driver.h"
#include "pci.h"

byte* video_mem;

#define PCI_VENDOR_ID_S3				0x5333
#define PCI_DEVICE_ID_S3_TRIO			0x8811

/*
#define mem_in8(addr)			in((word)(addr - s3_base - ))
#define mem_in16(addr)		  in16((void *)(addr))
#define mem_in32(addr)			in32((void *)(addr))

#define mem_out8(val, addr) 	out((void *)(addr), val)
#define mem_out16(val, addr)	  out16((void *)(addr), val)
#define mem_out32(val, addr)	out32((void *)(addr), val)
*/

#define IO_OUT16VAL(v, r)		(((v) << 8) | (r))


//static int currcon = 0;
//static struct display disp;
//static struct fb_info fb_info;
//static struct { byte red, green, blue, pad; } palette[256];
static wchar_t s3trio_name[16] = L"S3Trio ";
static byte *s3trio_base;

static struct fb_fix_screeninfo fb_fix;
static struct fb_var_screeninfo fb_var = { 0, };


/*
*  Interface used by the world
*/

static int s3trio_open(struct fb_info *info, int user);
static int s3trio_release(struct fb_info *info, int user);
static int s3trio_get_fix(struct fb_fix_screeninfo *fix, int con,
						  struct fb_info *info);
static int s3trio_get_var(struct fb_var_screeninfo *var, int con,
						  struct fb_info *info);
static int s3trio_set_var(struct fb_var_screeninfo *var, int con,
						  struct fb_info *info);
static int s3trio_get_cmap(struct fb_cmap *cmap, int kspc, int con,
						   struct fb_info *info);
static int s3trio_set_cmap(struct fb_cmap *cmap, int kspc, int con,
						   struct fb_info *info);
static int s3trio_pan_display(struct fb_var_screeninfo *var, int con,
							  struct fb_info *info);
static int s3trio_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
						dword arg, int con, struct fb_info *info);


						/*
						*  Interface to the low level console driver
*/

void s3triofb_init(void);
static int s3triofbcon_switch(int con, struct fb_info *info);
static int s3triofbcon_updatevar(int con, struct fb_info *info);
static void s3triofbcon_blank(int blank, struct fb_info *info);
#if 0
static int s3triofbcon_setcmap(struct fb_cmap *cmap, int con);
#endif

/*
*  Text console acceleration
*/

#ifdef FBCON_HAS_CFB8
static struct display_switch fbcon_trio8;
#endif

/*
*	 Accelerated Functions used by the low level console driver
*/

static void Trio_WaitQueue(word fifo);
static void Trio_WaitBlit(void);
static void Trio_BitBLT(word curx, word cury, word destx,
						word desty, word width, word height,
						word mode);
static void Trio_RectFill(word x, word y, word width, word height,
						  word mode, word color);
static void Trio_MoveCursor(word x, word y);


/*
*  Internal routines
*/

static int s3trio_getcolreg(unsigned int regno, unsigned int *red, unsigned int *green, unsigned int *blue,
							unsigned int *transp, struct fb_info *info);
static int s3trio_setcolreg(unsigned int regno, unsigned int red, unsigned int green, unsigned int blue,
							unsigned int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);


/*static struct fb_ops s3trio_ops = {
s3trio_open, s3trio_release, s3trio_get_fix, s3trio_get_var, s3trio_set_var,
s3trio_get_cmap, s3trio_set_cmap, s3trio_pan_display, s3trio_ioctl
};*/


/*
*  Open/Release the frame buffer device
*/

static int s3trio_open(struct fb_info *info, int user)

{
/*
*  Nothing, only a usage count for the moment
	*/
	
	//MOD_INC_USE_COUNT;
	return(0);
}

static int s3trio_release(struct fb_info *info, int user)

{
	//MOD_DEC_USE_COUNT;
	return(0);
}


/*
*  Get the Fixed Part of the Display
*/

static int s3trio_get_fix(struct fb_fix_screeninfo *fix, int con,
						  
						  struct fb_info *info)
{
	memcpy(fix, &fb_fix, sizeof(fb_fix));
	return 0;
}


/*
*  Get the User Defined Part of the Display
*/

static int s3trio_get_var(struct fb_var_screeninfo *var, int con,
						  
						  struct fb_info *info)
{
	memcpy(var, &fb_var, sizeof(fb_var));
	return 0;
}


/*
*  Set the User Defined Part of the Display
*/

static int s3trio_set_var(struct fb_var_screeninfo *var, int con,
						  
						  struct fb_info *info)
{
	if (var->xres > fb_var.xres || var->yres > fb_var.yres ||
		var->bits_per_pixel > fb_var.bits_per_pixel )
		/* || var->nonstd || var->vmode != FB_VMODE_NONINTERLACED) */
		return 1;
	if (var->xres_virtual > fb_var.xres_virtual) {
		out16(0x3d4, IO_OUT16VAL((var->xres_virtual /8) & 0xff, 0x13));
		out16(0x3d4, IO_OUT16VAL(((var->xres_virtual /8 ) & 0x300) >> 3, 0x51));
		fb_var.xres_virtual = var->xres_virtual;
		fb_fix.line_length = var->xres_virtual;
	}
	fb_var.yres_virtual = var->yres_virtual;
	memcpy(var, &fb_var, sizeof(fb_var));
	return 0;
}


/*
*  Pan or Wrap the Display
*
*  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
*/

static int s3trio_pan_display(struct fb_var_screeninfo *var, int con,
							  
							  struct fb_info *info)
{
	unsigned int base;
	
	if (var->xoffset > (var->xres_virtual - var->xres))
		return 1;
	if (var->yoffset > (var->yres_virtual - var->yres))
		return 1;
	
	fb_var.xoffset = var->xoffset;
	fb_var.yoffset = var->yoffset;
	
	base = var->yoffset * fb_fix.line_length + var->xoffset;
	
	out16(0x03D4, IO_OUT16VAL((base >> 8) & 0xff, 0x0c));
	out16(0x03D4, IO_OUT16VAL(base	& 0xff, 0x0d));
	out16(0x03D4, IO_OUT16VAL((base >> 16) & 0xf, 0x69));
	return 0;
}


/*
*  Get the Colormap
*/

static int s3trio_get_cmap(struct fb_cmap *cmap, int kspc, int con,
						   
						   struct fb_info *info)
{
#if 0
	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, s3trio_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else
		fb_copy_cmap(fb_default_cmap(1 << fb_display[con].var.bits_per_pixel),
		cmap, kspc ? 0 : 2);
#endif
	return 0;
}

/*
*  Set the Colormap
*/

static int s3trio_set_cmap(struct fb_cmap *cmap, int kspc, int con,
						   
						   struct fb_info *info)
{
#if 0
	int err;
	
	
	if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
			1<<fb_display[con].var.bits_per_pixel, 0)))
			return err;
	}
	if (con == currcon) 				/* current console? */
		return fb_set_cmap(cmap, kspc, s3trio_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
#endif
	return 0;
}


static int s3trio_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
						
						dword arg, int con, struct fb_info *info)
{
	return 1;
}

void s3triofb_init(void)

{
#ifdef __powerpc__
	/* We don't want to be called like this. */
	/* We rely on Open Firmware (offb) instead. */
#else /* !__powerpc__ */
	/* To be merged with cybervision */
#endif /* !__powerpc__ */
}

void s3trio_resetaccel(void) {
	
	
	
#define EC01_ENH_ENB	0x0005
#define EC01_LAW_ENB	0x0010
#define EC01_MMIO_ENB	0x0020
	
#define EC00_RESET		0x8000
#define EC00_ENABLE 	0x4000
#define MF_MULT_MISC	0xE000
#define SRC_FOREGROUND	0x0020
#define SRC_BACKGROUND	0x0000
#define MIX_SRC 				0x0007
#define MF_T_CLIP		0x1000
#define MF_L_CLIP		0x2000
#define MF_B_CLIP		0x3000
#define MF_R_CLIP		0x4000
#define MF_PIX_CONTROL	0xA000
#define MFA_SRC_FOREGR_MIX		0x0000
#define MF_PIX_CONTROL	0xA000
	
	out16(0x42e8, EC00_RESET);
	in16(0x42e8);
	out16(0x42e8, EC00_ENABLE);
	in16(0x42e8);
	out16(0x4ae8, EC01_ENH_ENB | EC01_LAW_ENB);
	out16(0xbee8, MF_MULT_MISC); /* 16 bit I/O registers */
	
	/* Now set some basic accelerator registers */
	Trio_WaitQueue(0x0400);
	out16(0xbae8, SRC_FOREGROUND | MIX_SRC);
	out16(0xb6e8, SRC_BACKGROUND | MIX_SRC);/* direct color*/
	out16(0xbee8, MF_T_CLIP | 0);	  /* clip virtual area	*/
	out16(0xbee8, MF_L_CLIP | 0);
	out16(0xbee8, MF_R_CLIP | (640 - 1));
	out16(0xbee8, MF_B_CLIP | (480 - 1));
	Trio_WaitQueue(0x0400);
	out16(0xaae8, 0xffff);		 /* Enable all planes */
	out16(0xaae8, 0xffff);		 /* Enable all planes */
	out16(0xbee8, MF_PIX_CONTROL | MFA_SRC_FOREGR_MIX);
}

int s3trio_init(struct device_node *dp)
{
	
	
	//byte bus, dev;
	//unsigned int t32;
	//unsigned short cmd;
	pci_cfg_t cfg;
	
	/*pci_device_loc(dp,&bus,&dev);
	pcibios_read_config_dword(bus, dev, PCI_VENDOR_ID, &t32);
	if(t32 == (PCI_DEVICE_ID_S3_TRIO << 16) + PCI_VENDOR_ID_S3)*/
	if (pci_find(PCI_VENDOR_ID_S3, PCI_DEVICE_ID_S3_TRIO, &cfg))
	{
		//pcibios_read_config_dword(bus, dev, PCI_BASE_ADDRESS_0, &t32);
		//pcibios_read_config_dword(bus, dev, PCI_BASE_ADDRESS_1, &t32);
		//pcibios_read_config_word(bus, dev, PCI_COMMAND,&cmd);
		
		//pcibios_write_config_word(bus, dev, PCI_COMMAND, PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
		
		//pcibios_write_config_dword(bus, dev, PCI_BASE_ADDRESS_0,0xffffffff);
		//pcibios_read_config_dword(bus, dev, PCI_BASE_ADDRESS_0, &t32);
		
		/* This is a gross hack as OF only maps enough memory for the framebuffer and
		we want to use MMIO too. We should find out which chunk of address space
		we can use here */
		//pcibios_write_config_dword(bus,dev,PCI_BASE_ADDRESS_0,0xc6000000);
		
		wprintf(L"S3 frame buffer at %x\n", cfg.base[0]);
		video_mem = (byte*) cfg.base[0];
		
		/* unlock s3 */
		
		out(0x3C3, 0x01);
		
		out(0x3c2, (byte) (in(0x03CC) | 1));
		
		out16(0x03D4, IO_OUT16VAL(0x48, 0x38));
		out16(0x03D4, IO_OUT16VAL(0xA0, 0x39));
		out(0x3d4, 0x33);
		out16(0x3d4, 
			(word) (IO_OUT16VAL((in(0x3d5) & ~(0x2 | 0x10 |  0x40)) | 0x20, 0x33)));
		
		out16(0x3c4, IO_OUT16VAL(0x6, 0x8));
		
		/* switch to MMIO only mode */
		
		out(0x3d4, 0x58);
		out16(0x3d4, (word) IO_OUT16VAL(in(0x3d5) | 3 | 0x10, 0x58));
		out16(0x3d4, (word) IO_OUT16VAL(8, 0x53));
		
		/* switch off I/O accesses */
		
#if 0
		pcibios_write_config_word(bus, dev, PCI_COMMAND,
			PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
#endif
		return 1;
	}
	
	return 0;
}

#include "palette.h"

/*
*  Initialisation
*  We heavily rely on OF for the moment. This needs fixing.
*/

void s3triofb_init_of(struct device_node *dp)
{
	int /*i, *pp, len, */x, y;
	unsigned long address;
	//dword *CursorBase;
	pci_cfg_t cfg;
	
	if (!pci_find(PCI_VENDOR_ID_S3, PCI_DEVICE_ID_S3_TRIO, &cfg))
	{
		wprintf(L"Can't find S3 Trio64 card\n");
		return;
	}
	
	//wcscat(s3trio_name, dp->name);
	//s3trio_name[countof(s3trio_name)-1] = '\0';
	wcscpy(fb_fix.id, s3trio_name);
	
	/*if((pp = (int *)get_property(dp, "vendor-id", &len)) != NULL
	&& *pp!=PCI_VENDOR_ID_S3) {
	printk("%s: can't find S3 Trio board\n", dp->full_name);
	return;
	}
	
	  if((pp = (int *)get_property(dp, "device-id", &len)) != NULL
	  && *pp!=PCI_DEVICE_ID_S3_TRIO) {
	  printk("%s: can't find S3 Trio board\n", dp->full_name);
	  return;
	  }
	  
		if ((pp = (int *)get_property(dp, "depth", &len)) != NULL
		&& len == sizeof(int) && *pp != 8) {
		printk("%s: can't use depth = %d\n", dp->full_name, *pp);
		return;
		}
		if ((pp = (int *)get_property(dp, "width", &len)) != NULL
		&& len == sizeof(int))
		fb_var.xres = fb_var.xres_virtual = *pp;
		if ((pp = (int *)get_property(dp, "height", &len)) != NULL
		&& len == sizeof(int))
		fb_var.yres = fb_var.yres_virtual = *pp;
		if ((pp = (int *)get_property(dp, "linebytes", &len)) != NULL
		&& len == sizeof(int))
		fb_fix.line_length = *pp;
		else
		fb_fix.line_length = fb_var.xres_virtual;
	fb_fix.smem_len = fb_fix.line_length*fb_var.yres;*/
	
	fb_var.xres = fb_var.xres_virtual = 640;
	fb_var.yres = fb_var.yres_virtual = 480;
	fb_fix.line_length = fb_var.xres_virtual;
	fb_fix.smem_len = fb_fix.line_length * fb_var.yres;
	
	s3trio_init(dp);
	s3trio_base = video_mem;
	address = (unsigned long) video_mem;
	//address = 0xc6000000;
	//s3trio_base = ioremap(address,64*1024*1024);
	fb_fix.smem_start = (char *)address;
	fb_fix.type = FB_TYPE_PACKED_PIXELS;
	fb_fix.type_aux = 0;
	fb_fix.accel = FB_ACCEL_S3_TRIO64;
	fb_fix.mmio_start = (char *)address+0x1000000;
	fb_fix.mmio_len = 0x1000000;
	
	fb_fix.xpanstep = 1;
	fb_fix.ypanstep = 1;
	
	s3trio_resetaccel();
	
	out(0x03D4, 0x30);
	out(0x03D4, 0x2d);
	out(0x03D4, 0x2e);
	
	out(0x03D4, 0x50);
	
	/* disable HW cursor */
	
	out(0x03D4, 0x39);
	out(0x03D5, 0xa0);
	
	out(0x03D4, 0x45);
	out(0x03D5, 0);
	
	out(0x03D4, 0x4e);
	out(0x03D5, 0);
	
	out(0x03D4, 0x4f);
	out(0x03D5, 0);
	
	/* init HW cursor */
	
	/*CursorBase = (dword *)(s3trio_base + 2*1024*1024 - 0x400);
	for (i = 0; i < 8; i++) {
	*(CursorBase  +(i*4)) = 0xffffff00;
	*(CursorBase+1+(i*4)) = 0xffff0000;
	*(CursorBase+2+(i*4)) = 0xffff0000;
	*(CursorBase+3+(i*4)) = 0xffff0000;
	}
	for (i = 8; i < 64; i++) {
	*(CursorBase  +(i*4)) = 0xffff0000;
	*(CursorBase+1+(i*4)) = 0xffff0000;
	*(CursorBase+2+(i*4)) = 0xffff0000;
	*(CursorBase+3+(i*4)) = 0xffff0000;
		}*/
	
	
	out(0x03D4, 0x4c);
	out(0x03D5, ((2*1024 - 1)&0xf00)>>8);
	
	out(0x03D4, 0x4d);
	out(0x03D5, (2*1024 - 1) & 0xff);
	
	out(0x03D4, 0x45);
	in(0x03D4);
	
	out(0x03D4, 0x4a);
	out(0x03D5, 0x80);
	out(0x03D5, 0x80);
	out(0x03D5, 0x80);
	
	out(0x03D4, 0x4b);
	out(0x03D5, 0x00);
	out(0x03D5, 0x00);
	out(0x03D5, 0x00);
	
	out(0x03D4, 0x45);
	out(0x03D5, 0);
	
	/* setup default color table */
	
	/*for(i = 0; i < 16; i++) {
	int j = color_table[i];
	palette[i].red=default_red[j];
	palette[i].green=default_grn[j];
	palette[i].blue=default_blu[j];
		}*/
	
	s3trio_setcolreg(255, 56, 100, 160, 0, NULL /* not used */);
	s3trio_setcolreg(254, 0, 0, 0, 0, NULL /* not used */);
	//memset((char *)s3trio_base, 2, 640*480);
	strcpy((char *)s3trio_base, "H   e   l   l   o   ,       w   o   r   l   d   ");
	
#if 0
	Trio_RectFill(0, 0, 90, 90, 7, 1);
#endif
	
	fb_fix.visual = FB_VISUAL_PSEUDOCOLOR ;
	fb_var.xoffset = fb_var.yoffset = 0;
	fb_var.bits_per_pixel = 8;
	fb_var.grayscale = 0;
	fb_var.red.offset = fb_var.green.offset = fb_var.blue.offset = 0;
	fb_var.red.length = fb_var.green.length = fb_var.blue.length = 8;
	fb_var.red.msb_right = fb_var.green.msb_right = fb_var.blue.msb_right = 0;
	fb_var.transp.offset = fb_var.transp.length = fb_var.transp.msb_right = 0;
	fb_var.nonstd = 0;
	fb_var.activate = 0;
	fb_var.height = fb_var.width = -1;
	fb_var.accel_flags = FB_ACCELF_TEXT;
	fb_var.pixclock = 1;
	fb_var.left_margin = fb_var.right_margin = 0;
	fb_var.upper_margin = fb_var.lower_margin = 0;
	fb_var.hsync_len = fb_var.vsync_len = 0;
	fb_var.sync = 0;
	fb_var.vmode = FB_VMODE_NONINTERLACED;
	
	/*disp.var = fb_var;
	disp.cmap.start = 0;
	disp.cmap.len = 0;
	disp.cmap.red = disp.cmap.green = disp.cmap.blue = disp.cmap.transp = NULL;
	disp.screen_base = s3trio_base;
	disp.visual = fb_fix.visual;
	disp.type = fb_fix.type;
	disp.type_aux = fb_fix.type_aux;
	disp.ypanstep = 0;
	disp.ywrapstep = 0;
	disp.line_length = fb_fix.line_length;
	disp.can_soft_blank = 1;
	disp.inverse = 0;*/
#ifdef FBCON_HAS_CFB8
	if (fb_var.accel_flags & FB_ACCELF_TEXT)
		disp.dispsw = &fbcon_trio8;
	else
		disp.dispsw = &fbcon_cfb8;
#else
	//disp.dispsw = &fbcon_dummy;
#endif
	//disp.scrollmode = fb_var.accel_flags & FB_ACCELF_TEXT ? 0 : SCROLL_YREDRAW;
	
	/*strcpy(fb_info.modename, "Trio64 ");
	strncat(fb_info.modename, dp->full_name, sizeof(fb_info.modename));
	fb_info.node = -1;
	fb_info.fbops = &s3trio_ops;*/
#if 0
	fb_info.fbvar_num = 1;
	fb_info.fbvar = &fb_var;
#endif
	/*fb_info.disp = &disp;
	fb_info.fontname[0] = '\0';
	fb_info.changevar = NULL;
	fb_info.switch_con = &s3triofbcon_switch;
	fb_info.updatevar = &s3triofbcon_updatevar;
	fb_info.blank = &s3triofbcon_blank;*/
#if 0
	fb_info.setcmap = &s3triofbcon_setcmap;
#endif
	
#ifdef CONFIG_FB_COMPAT_XPMAC
	if (!console_fb_info) {
		display_info.height = fb_var.yres;
		display_info.width = fb_var.xres;
		display_info.depth = 8;
		display_info.pitch = fb_fix.line_length;
		display_info.mode = 0;
		strncpy(display_info.name, dp->name, sizeof(display_info.name));
		display_info.fb_address = (unsigned long)fb_fix.smem_start;
		display_info.disp_reg_address = address + 0x1008000;
		display_info.cmap_adr_address = address + 0x1008000 + 0x3c8;
		display_info.cmap_data_address = address + 0x1008000 + 0x3c9;
		console_fb_info = &fb_info;
	}
#endif /* CONFIG_FB_COMPAT_XPMAC) */
	
	s3trio_set_var(&fb_var, 0, NULL);
	/*fb_info.flags = FBINFO_FLAG_DEFAULT;
	if (register_framebuffer(&fb_info) < 0)
	return;*/
}


static int s3triofbcon_switch(int con, struct fb_info *info)

{
#if 0
	/* Do we have to save the colormap? */
	if (fb_display[currcon].cmap.len)
		fb_get_cmap(&fb_display[currcon].cmap, 1, s3trio_getcolreg, info);
	
	currcon = con;
	/* Install new colormap */
	do_install_cmap(con,info);
#endif
	return 0;
}

/*
*  Update the `var' structure (called by fbcon.c)
*/

static int s3triofbcon_updatevar(int con, struct fb_info *info)

{
	/* Nothing */
	return 0;
}

/*
*  Blank the display.
*/

static void s3triofbcon_blank(int blank, struct fb_info *info)

{
	unsigned char x;
	
	out(0x03c4, 0x1);
	x = in(0x03c5);
	out(0x03c5, (x & (~0x20)) | (blank << 5));
}

/*
*  Set the colormap
*/

#if 0
static int s3triofbcon_setcmap(struct fb_cmap *cmap, int con)

{
	return(s3trio_set_cmap(cmap, 1, con, &fb_info));
}
#endif


/*
*  Read a single color register and split it into
*  colors/transparent. Return != 0 for invalid regno.
*/

static int s3trio_getcolreg(unsigned int regno, unsigned int *red, unsigned int *green, unsigned int *blue,
							
							unsigned int *transp, struct fb_info *info)
{
	if (regno > 255)
		return 1;
	*red = (palette[regno * 3 + 0] << 8) | palette[regno * 3 + 0];
	*green = (palette[regno * 3 + 1] << 8) | palette[regno * 3 + 1];
	*blue = (palette[regno * 3 + 2] << 8) | palette[regno * 3 + 2];
	*transp = 0;
	return 0;
}


/*
*  Set a single color register. Return != 0 for invalid regno.
*/

static int s3trio_setcolreg(unsigned int regno, unsigned int red, unsigned int green, unsigned int blue,
							
							unsigned int transp, struct fb_info *info)
{
	if (regno > 255)
		return 1;
	
	red >>= 8;
	green >>= 8;
	blue >>= 8;
	palette[regno * 3 + 0] = red;
	palette[regno * 3 + 1] = green;
	palette[regno * 3 + 2] = blue;
	
	out(0x3c8, (byte) regno);
	out(0x3c9, (byte) ((red & 0xff) >> 2));
	out(0x3c9, (byte) ((green & 0xff) >> 2));
	out(0x3c9, (byte) ((blue & 0xff) >> 2));
	
	return 0;
}


static void do_install_cmap(int con, struct fb_info *info)

{
#if 0
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, s3trio_setcolreg, &fb_info);
	else
		fb_set_cmap(fb_default_cmap(fb_display[con].var.bits_per_pixel), 1,
		s3trio_setcolreg, &fb_info);
#endif
}

void s3triofb_setup(char *options, int *ints) {
	
	
	return;
	
}

static void Trio_WaitQueue(word fifo) {
	
	
	word status;
	
	do
	{
		status = in16(0x9AE8);
	}  while (!(status & fifo));
	
}

static void Trio_WaitBlit(void) {
	
	
	word status;
	
	do
	{
		status = in16(0x9AE8);
	}  while (status & 0x200);
	
}

static void Trio_BitBLT(word curx, word cury, word destx,
						
						word desty, word width, word height,
						word mode) {
	
	word blitcmd = 0xc011;
	
	/* Set drawing direction */
	/* -Y, X maj, -X (default) */
	
	if (curx > destx)
		blitcmd |= 0x0020;	/* Drawing direction +X */
	else {
		curx  += (width - 1);
		destx += (width - 1);
	}
	
	if (cury > desty)
		blitcmd |= 0x0080;	/* Drawing direction +Y */
	else {
		cury  += (height - 1);
		desty += (height - 1);
	}
	
	Trio_WaitQueue(0x0400);
	
	out16(0xBEE8, 0xa000);
	out16(0xBAE8, 0x60 | mode);
	
	out16(0x86E8, curx);
	out16(0x82E8, cury);
	
	out16(0x8EE8, destx);
	out16(0x8AE8, desty);
	
	out16(0xBEE8, height - 1);
	out16(0x96E8, width - 1);
	
	out16(0x9AE8, blitcmd);
	
}

static void Trio_RectFill(word x, word y, word width, word height, word mode, word color)
{
	word blitcmd = 0x40b1;
	
	Trio_WaitQueue(0x0400);
	
	out16(0xBEE8, 0xa000);
	out16(0xBAE8, (0x20 | mode));
	out16(0xBEE8, 0xe000);
	out16(0xA6E8, color);
	out16(0x86E8, x);
	out16(0x82E8, y);
	out16(0xBEE8, (height - 1));
	out16(0x96E8, (width - 1));
	out16(0x9AE8, blitcmd);
}


static void Trio_MoveCursor(word x, word y) {
	
	
	out(0x3d4, 0x39);
	out(0x3d5, 0xa0);
	
	out(0x3d4, 0x46);
	out(0x3d5, (x & 0x0700) >> 8);
	out(0x3d4, 0x47);
	out(0x3d5, x & 0x00ff);
	
	out(0x3d4, 0x48);
	out(0x3d5, (y & 0x0700) >> 8);
	out(0x3d4, 0x49);
	out(0x3d5, y & 0x00ff);
	
}


/*
*  Text console acceleration
*/

#ifdef FBCON_HAS_CFB8
static void fbcon_trio8_bmove(struct display *p, int sy, int sx, int dy,
							  
							  int dx, int height, int width)
{
	sx *= 8; dx *= 8; width *= 8;
	Trio_BitBLT((word)sx, (word)(sy*fontheight(p)), (word)dx,
		(word)(dy*fontheight(p)), (word)width,
		(word)(height*fontheight(p)), (word)S3_NEW);
}

static void fbcon_trio8_clear(struct vc_data *conp, struct display *p, int sy,
							  
							  int sx, int height, int width)
{
	unsigned char bg;
	
	sx *= 8; width *= 8;
	bg = attr_bgcol_ec(p,conp);
	Trio_RectFill((word)sx,
		(word)(sy*fontheight(p)),
		(word)width,
		(word)(height*fontheight(p)),
		(word)S3_NEW,
		(word)bg);
}

static void fbcon_trio8_putc(struct vc_data *conp, struct display *p, int c,
							 
							 int yy, int xx)
{
	Trio_WaitBlit();
	fbcon_cfb8_putc(conp, p, c, yy, xx);
}

static void fbcon_trio8_putcs(struct vc_data *conp, struct display *p,
							  
							  const unsigned short *s, int count, int yy, int xx)
{
	Trio_WaitBlit();
	fbcon_cfb8_putcs(conp, p, s, count, yy, xx);
}

static void fbcon_trio8_revc(struct display *p, int xx, int yy)

{
	Trio_WaitBlit();
	fbcon_cfb8_revc(p, xx, yy);
}

static struct display_switch fbcon_trio8 = {
	fbcon_cfb8_setup, fbcon_trio8_bmove, fbcon_trio8_clear, fbcon_trio8_putc,
		fbcon_trio8_putcs, fbcon_trio8_revc, NULL, NULL, fbcon_cfb8_clear_margins,
		FONTWIDTH(8)
};
#endif