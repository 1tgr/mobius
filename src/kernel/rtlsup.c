/* $Id: rtlsup.c,v 1.5 2002/01/06 22:46:09 pavlovskii Exp $ */

#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/proc.h>

#include <os/rtl.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

extern addr_t kernel_pagedir[];

addr_t kernel_sbrk = 0xe0000000;
unsigned con_x, con_y;
uint16_t con_attribs = 0x0700;

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

static void TextUpdateCursor(void)
{
	unsigned short Off;

	Off = (con_y * 80 + con_x) * 2;
	/* extra shift because even/odd text mode uses word clocking */
	out(VGA_CRTC_INDEX, 14);
	out(VGA_CRTC_DATA, Off >> 9);
	out(VGA_CRTC_INDEX, 15);
	out(VGA_CRTC_DATA, Off >> 1);
}

static void TextSwitchToKernel(void)
{
	out(VGA_CRTC_INDEX, 12);
	out(VGA_CRTC_DATA, 0);
	out(VGA_CRTC_INDEX, 13);
	out(VGA_CRTC_DATA, 0);
	TextUpdateCursor();
}

void __dj_assert(const char *test, const char *file, int line)
{
	TextSwitchToKernel();
	wprintf(L"Assertion failed!\n"
		L"%S\n"
		L"%S, line %d\n", 
		test, file, line);
	for (;;);
	/*__asm__("cli;hlt");*/
}

#define DEFINE_PUTS(name, ct) \
int name(const ct *str, size_t count) \
{ \
	uint16_t *mem = (uint16_t*) PHYSICAL(0xb8000); \
\
	for (; *str && count > 0; count--, str++) \
	{ \
		switch (*str) \
		{ \
		case '\n': \
			con_y++; \
		case '\r': \
			con_x = 0; \
			break; \
 \
		case '\b': \
			if (con_x > 0) \
				con_x--; \
			break; \
 \
		case '\t': \
			con_x = (con_x + 4) & ~3; \
			break; \
 \
		default: \
			mem[con_x + con_y * 80] =  \
				(uint16_t) (uint8_t) *str | con_attribs; \
			con_x++; \
		} \
 \
		if (con_x >= 80) \
		{ \
			con_x = 0; \
			con_y++; \
		} \
 \
		while (con_y >= 25) \
		{ \
			unsigned i; \
 \
			memmove(mem, mem + 80, 80 * 24 * 2); \
			for (i = 0; i < 80; i++) \
				mem[80 * 24 + i] = ' ' | con_attribs; \
 \
			con_y--; \
		} \
	} \
 \
	TextUpdateCursor(); \
	return 0; \
}

DEFINE_PUTS(_cputws, wchar_t);
DEFINE_PUTS(_cputs, char);

wchar_t *ProcGetCwd()
{
	return current->process->info->cwd;
}

char *sbrk(size_t diff)
{
	addr_t new_sbrk, start, phys;

	diff = PAGE_ALIGN_UP(diff);
	if (kernel_sbrk + diff >= 0xf0000000)
		return (char*) -1;

	start = kernel_sbrk;
	new_sbrk = kernel_sbrk + diff;

	wprintf(L"sbrk: getting %d bytes at %lx", diff, start);
	for (; kernel_sbrk < new_sbrk; kernel_sbrk += PAGE_SIZE)
	{
		phys = MemAlloc();
		if (phys == NULL)
			return (char*) -1;

		/*wprintf(L"%lx=>%lx ", kernel_sbrk, phys);*/
		_cputws(L".", 1);
		if (!MemMap(kernel_sbrk, phys, kernel_sbrk + PAGE_SIZE, 
			PRIV_WR | PRIV_KERN | PRIV_PRES))
			return (char*) -1;
	}

	wprintf(L"done\n");
	return (char*) start;
}

void *sbrk_virtual(size_t diff)
{
	addr_t start;

	diff = PAGE_ALIGN_UP(diff);
	if (kernel_sbrk + diff >= 0xf0000000)
		return NULL;

	start = kernel_sbrk;
	kernel_sbrk += diff;
	return (void*) start;
}
