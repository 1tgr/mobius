#ifndef __I386_H
#define __I386_H

#include <sys/types.h>

/* Ports */
#define PORT_8259M      0x20
#define PORT_8259S      0xA0
#define PORT_KBD_A      0x60

#define PORT_8253_0     0x40
#define PORT_8253_1     0x41
#define PORT_8253_2     0x42
#define PORT_8253_MODE  0x43

#define EOI             0x20

/* Access byte's flags */
#define ACS_PRESENT     0x80            /* present segment */
#define ACS_CSEG        0x18            /* code segment */
#define ACS_DSEG        0x10            /* data segment */
#define ACS_CONFORM     0x04            /* conforming segment */
#define ACS_READ        0x02            /* readable segment */
#define ACS_WRITE       0x02            /* writable segment */
#define ACS_IDT         ACS_DSEG        /* segment type is the same type */
#define ACS_INT_GATE    0x0E            /* int gate for 386 */
#define ACS_TSS_GATE    0x09
#define ACS_DPL_0       0x00            /* descriptor privilege level #0 */
#define ACS_DPL_1       0x20            /* descriptor privilege level #1 */
#define ACS_DPL_2       0x40            /* descriptor privilege level #2 */
#define ACS_DPL_3       0x60            /* descriptor privilege level #3 */
#define ACS_LDT         0x02            /* ldt descriptor */
#define ACS_TASK_GATE   0x05

/* Ready-made values */
#define ACS_INT         (ACS_PRESENT | ACS_INT_GATE) /* present int gate */
#define ACS_TSS         (ACS_PRESENT | ACS_TSS_GATE) /* present tss gate */
#define ACS_TASK        (ACS_PRESENT | ACS_TASK_GATE) /* present task gate */
#define ACS_CODE        (ACS_PRESENT | ACS_CSEG | ACS_READ)
#define ACS_DATA        (ACS_PRESENT | ACS_DSEG | ACS_WRITE)
#define ACS_STACK       (ACS_PRESENT | ACS_DSEG | ACS_WRITE)

/* Attributes (in terms of size) */
#define ATTR_GRANULARITY 0x80           /* segment limit is given in 4KB pages rather than in bytes */
#define ATTR_BIG         0x40           /* ESP is used rather than SP */
#define ATTR_DEFAULT     0x40           /* 32-bit code segment rather than 16-bit */

/* Paging */
#define PAGE_BITS       12
#define PAGE_SIZE       (1 << PAGE_BITS)

#define PRIV_PRES       0x001   // present
#define PRIV_RD         0x000   // read-only
#define PRIV_WR         0x002   // writable
#define PRIV_KERN       0x000   // ring 0
#define PRIV_USER       0x004   // ring 3
#define PRIV_ALL        0x007

#define PAGE_NUM(X)     ((X) & -PAGE_SIZE)
#define PAGE_TABENT(X)  (((X) >> 12) & 0x3FF)
#define PAGE_DIRENT(X)  (((X) >> 22) & 0x3FF)

#define EFLAG_TF	0x100
#define EFLAG_IF	0x200
#define EFLAG_IOPL0	0x0000
#define EFLAG_IOPL1	0x1000
#define EFLAG_IOPL2	0x2000
#define EFLAG_IOPL3	0x3000
#define EFLAG_VM	0x20000

#pragma pack (push, 1)  /* align structures to a byte boundary */

/* Segment desciptor definition */
struct descriptor_t
{
	word limit,
		base_l;
	byte base_m,
		access,
		attribs,
		base_h;
};

typedef struct descriptor_t descriptor_t;

struct dtr_t
{
	word limit;
	dword base;
};

typedef struct dtr_t gdtr_t;   /* GDTR register definition */
typedef struct dtr_t idtr_t;   /* IDTR register definition */

/* Interrupt desciptor definition */
struct descriptor_int_t
{
	word offset_l,
		selector;

	byte param_cnt, access;
	word offset_h;
};

typedef struct descriptor_int_t descriptor_int_t;

typedef struct pusha_t pusha_t;
struct pusha_t
{
	dword edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

/* TSS definition */
struct tss_t     /* TSS for 386+ */
{ 
	dword link,
		esp0,
		ss0,
		esp1,
		ss1,
		esp2,
		ss2,
		cr3,
		eip,
		eflags,
		eax,
		ecx,
		edx,
		ebx,
		esp,
		ebp,
		esi,
		edi,
		es,
		cs,
		ss,
		ds,
		fs,
		gs,
		ldtr;
	word  trace,
		io_map_addr;
	byte  io_map[8192];
};

typedef struct tss_t tss_t;

struct tss0_t  /* TSS for 386+ (no I/O bit map) */
{
	dword link,
		esp0,
		ss0,
		esp1,
		ss1,
		esp2,
		ss2,
		cr3,
		eip,
		eflags,
		eax,
		ecx,
		edx,
		ebx,
		esp,
		ebp,
		esi,
		edi;
	word es, u1,
		cs, u2,
		ss, u3,
		ds, u4,
		fs, u5,
		gs, u6,
		ldts, u7;
	word trace,
		io_map_addr;
};

#pragma pack (pop)      /* align structures to default boundary */

void	i386_set_descriptor(descriptor_t *item, dword base, dword limit, 
							byte access, byte attribs);
void	i386_set_descriptor_int(descriptor_int_t *item, word selector, 
								dword offset, byte access, byte param_cnt);

void	outs16(unsigned short Adr, unsigned short *Data, unsigned Count);

extern dword cpuid_ecx, cpuid_edx, cpu_max_level;
dword	keIdentifyCpu();

#ifdef _MSC_VER

#pragma warning(disable:4035)

/*
 * Visual C++ shouldn't really be used for building the kernel, so, since all
 * the following inline assembler functions are fairly priviliged, they
 * aren't defined for Visual C++.
 *
 * xxx - bugger - I have to write these now...
 */

#ifndef __cplusplus
#define inline
#endif

#if 0

static inline void enable() { __asm sti }
static void inline disable() { __asm cli }
inline static void invalidate_page(void* page) { __asm invlpg page }

static inline void i386_lpoke16(addr_t off, word value)
{
	__asm
	{
		mov ax, value
		mov word ptr gs:[off], ax
	}
}

static inline word i386_lpeek16(addr_t off)
{
	__asm movzx eax, word ptr gs:[off]
}

static inline void i386_lpoke32(addr_t off, dword value)
{
	__asm
	{
		mov eax, value
		mov dword ptr gs:[off], eax
	}
}

static inline word i386_lpeek32(addr_t off)
{
	__asm mov eax, dword ptr gs:[off]
}

static inline addr_t i386_lmemset32(addr_t off, dword c, size_t len)
{
	__asm
	{
		mov ecx, len
		shr ecx, 2
		mov eax, c
		mov edi, off
		cld
		rep stosd
	}
	return off;
}

static inline addr_t i386_lmemset16(addr_t off, word c, size_t len)
{
	__asm
	{
		mov ecx, len
		shr ecx, 1
		movzx eax, c
		mov edi, off
		cld
		rep stosw
	}
	return off;
}

static inline void out(word port, byte value)
{
	__asm
	{
		mov dx, port
		mov al, value
		out dx, al
	}
}

static inline void out16(word port, word value)
{
	__asm
	{
		mov dx, port
		mov ax, value
		out dx, ax
	}
}

static inline byte in(word port)
{
	__asm
	{
		xor eax, eax
		mov dx, port
		in al, dx
	}
}

static inline word in16(word port)
{
	__asm
	{
		xor eax, eax
		mov dx, port
		in ax, dx
	}
}

static inline void halt(dword code)
{
	__asm
	{
		mov eax, code
		cli
		hlt
	}
}

#endif

#else

#define inline __inline__

static inline void enable() { asm ("sti"); }
static inline void disable() { asm ("cli"); }
static inline void invalidate_page(void* page) { asm ("invlpg (%0)": :"r" (page)); }

static inline void i386_lpoke8(addr_t off, byte value)
{
	asm("movb %0,%%gs:(%1)" : : "r" (value), "r" (off));
}

static inline word i386_lpeek8(addr_t off)
{
	byte value;
	asm("movb %%gs:(%1),%0" : "=a" (value): "r" (off));
	return value;
}

static inline void i386_lpoke16(addr_t off, word value)
{
	asm("movw %0,%%gs:(%1)" : : "r" (value), "r" (off));
}

static inline word i386_lpeek16(addr_t off)
{
	word value;
	asm("movw %%gs:(%1),%0" : "=a" (value): "r" (off));
	return value;
}

static inline void i386_lpoke32(addr_t off, dword value)
{
	asm("movl %0,%%gs:(%1)" : : "r" (value), "r" (off));
}

static inline word i386_lpeek32(addr_t off)
{
	dword value;
	asm("movl %%gs:(%1),%0" : "=a" (value): "r" (off));
	return value;
}

static inline addr_t i386_lmemset32(addr_t off, dword c, size_t len)
{
	/*asm("mov	%0,%%ecx ; "
		"mov	%1,%%eax ; "
		"mov	%2,%%edi ; "
		"cld ; "
		"rep stosl": : "r" (len / 4), "r" (c), "r" (off): "eax", "ecx", "edi");*/
	__asm__("cld ; "
		"rep stosl": : "c" (len / 4), "a" (c), "D" (off): "eax", "ecx", "edi");
	return off;
}

static inline addr_t i386_lmemset16(addr_t off, word c, size_t len)
{
	/*asm("mov	%0,%%ecx ; "
		"movw	%1,%%ax ; "
		"mov	%2,%%edi ; "
		"cld ; "
		"rep stosw": : "r" (len / 2), "r" (c), "r" (off): "eax", "ecx", "edi");*/
	__asm__("cld ; "
		"rep stosw": : "c" (len / 2), "a" (c), "D" (off): "ax", "ecx", "edi");
	return off;
}

static inline addr_t i386_lmemset(addr_t off, char c, int count)
{
	__asm__("cld\n\t"
		"rep\n\t"
		"stosb"
		::"a" (c),"D" (off),"c" (count)
		:"cx","di");
	return off;
}

addr_t i386_llmemcpy(addr_t dest, addr_t src, size_t size);

#define out(port, value) \
	asm("outb	%%al, %%dx" : : "d" (port), "a" (value) : "eax", "edx")

static inline void out16(word port, word value)
{
	asm("outw	%%ax, %%dx" : : "d" (port), "a" (value));
}

static inline void out32(word port, dword value)
{
	asm("outl	%%eax, %%dx" : : "d" (port), "a" (value));
}

static inline byte in(word port)
{
	byte ret;
	asm("inb	%%dx,%%al" : "=a" (ret) : "d" (port));
	return ret;
}

static inline word in16(word port)
{
	word ret;
	asm("inw	%%dx,%%ax" : "=a" (ret) : "d" (port));
	return ret;
}

static inline dword in32(word port)
{
	dword ret;
	asm("inl	%%dx,%%eax" : "=a" (ret) : "d" (port));
	return ret;
}

static inline void halt(dword code)
{
	asm("cli ; hlt" : : "a" (code));
}

static inline dword critb(void)
{
	dword flags;
	asm("pushfl\n"
		"pop %0\n"
		"cli" : "=a" (flags));
	return flags;
}

static inline void crite(dword flags)
{
	asm("push %0\n"
		"popfl" : : "g" (flags));
}

#endif

#endif
