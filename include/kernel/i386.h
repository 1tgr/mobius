/* $Id: i386.h,v 1.1.1.1 2002/12/31 01:26:21 pavlovskii Exp $ */
#ifndef __KERNEL_I386_H
#define __KERNEL_I386_H

#include <sys/types.h>
#include <kernel/addresses.h>

/*!
 *	\ingroup	arch
 *	\defgroup	i386	Intel i386
 *	@{
 */

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

#ifdef PAGE_SIZE
#undef PAGE_SIZE
#endif

#define PAGE_SIZE          (1 << PAGE_BITS)

#define PRIV_PRES           0x001   /* present */
#define PRIV_RD             0x000   /* read-only */
#define PRIV_WR             0x002   /* writable */
#define PRIV_KERN           0x000   /* ring 0 */
#define PRIV_USER           0x004   /* ring 3 */
#define PRIV_ALL            0x007

#define PAGE_STATEMASK      0xf9b   /* mask out A, D and U/S bits */
#define PAGE_VALID_CLEAN   (0x000 | PRIV_PRES)
#define PAGE_VALID_DIRTY   (0x000 | PRIV_PRES | PRIV_WR)
#define PAGE_READINPROG     0x200
#define PAGE_WRITEINPROG   (0x400 | PRIV_PRES)
#define PAGE_READFAILED     0x600
#define PAGE_WRITEFAILED   (0x800 | PRIV_PRES)
#define PAGE_PAGEDOUT       0xa00

#define PRIV_WRITETHROUGH   0x008
#define PRIV_NOCACHE        0x010
#define PAGE_ACCESSED       0x020
#define PAGE_DIRTY          0x040   /* PTE only */
#define PAGE_4MEG           0x080   /* PDE only */
#define PAGE_GLOBAL         0x100   /* PTE only */

#define PAGE_NUM(X)         ((X) & -PAGE_SIZE)
#define PAGE_TABENT(X)      (((X) >> 12) & 0x3FF)
#define PAGE_DIRENT(X)      (((X) >> 22) & 0x3FF)

#define EFLAG_TF	    0x00100
#define EFLAG_IF	    0x00200
#define EFLAG_IOPL0	    0x00000
#define EFLAG_IOPL1	    0x01000
#define EFLAG_IOPL2	    0x02000
#define EFLAG_IOPL3	    0x03000
#define EFLAG_VM	    0x20000

#define DR6_BS		    0x4000

#define CR0_PG              0x80000000
#define CR0_CD              0x40000000
#define CR0_NW              0x20000000
#define CR0_AM              0x00040000
#define CR0_WP              0x00010000
#define CR0_NE              0x00000020
#define CR0_ET              0x00000010
#define CR0_TS              0x00000008
#define CR0_EM              0x00000004
#define CR0_MP              0x00000002
#define CR0_PE              0x00000001

#define CR3_PCD             0x00000010  /* ignored if CR0.CD set */
#define CR3_PWT             0x00000008  /* ignored if CR0.CD set */

#define CR4_PCE             0x00000100
#define CR4_PGE             0x00000080
#define CR4_MCE             0x00000040
#define CR4_PAE             0x00000020
#define CR4_PSE             0x00000010
#define CR4_DE              0x00000008
#define CR4_TSD             0x00000004
#define CR4_PVI             0x00000002
#define CR4_VME             0x00000001

/* Page fault error code flags */
#define PF_PROTECTED        1	/* protection violation... */
#define PF_WRITE            2	/* ...while writing */
#define PF_USER             4	/* ...in user mode */

#define SCHED_QUANTUM       10

#define ADDR_TO_PDE(v)     (addr_t*)(ADDR_PAGEDIRECTORY_VIRT + \
                                (((addr_t) (v) / (1024 * 1024))&(~0x3)))
#define ADDR_TO_PTE(v)     (addr_t*)(ADDR_PAGETABLE_VIRT + ((((addr_t) (v) / 1024))&(~0x3)))

#pragma pack (push, 1)  /* align structures to a byte boundary */

/* Segment desciptor definition */
struct descriptor_t
{
	uint16_t limit;
	uint16_t base_l;
	uint8_t base_m;
	uint8_t access;
	uint8_t attribs;
	uint8_t base_h;
};

typedef struct descriptor_t descriptor_t;

struct dtr_t
{
	uint16_t limit;
	uint32_t base;
};

typedef struct dtr_t gdtr_t;   /* GDTR register definition */
typedef struct dtr_t idtr_t;   /* IDTR register definition */

/* Interrupt desciptor definition */
struct descriptor_int_t
{
	uint16_t offset_l, selector;
	uint8_t param_cnt, access;
	uint16_t offset_h;
};

typedef struct descriptor_int_t descriptor_int_t;

#ifndef __CONTEXT_DEFINED
#define __CONTEXT_DEFINED
typedef struct pusha_t pusha_t;
struct pusha_t
{
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

/*! \brief The layout of the stack frame generated by the assembly-language 
 *		interrupt handler.
 *
 *	The isr_and_switch routine (in start.asm), along with the individual 
 *		interrupt handlers and the processor, build this structure on the stack
 *		before isr() is called.
 */
typedef struct context_t context_t;
struct context_t
{
	uint32_t kernel_esp;
	context_t *ctx_prev;
	uint32_t fault_addr;
	pusha_t regs;
	uint32_t gs, fs, es, ds;
	uint32_t intr, error;
	uint32_t eip, cs, eflags, esp, ss;
};

typedef struct context_v86_t context_v86_t;
struct context_v86_t
{
	uint32_t kernel_esp;
	context_t *ctx_prev;
	uint32_t fault_addr;
	pusha_t regs;
	uint32_t gs, fs, es, ds;
	uint32_t intr, error;
	uint32_t eip, cs, eflags, esp, ss;

	/* extra fields for V86 mode */
	uint32_t v86_es, v86_ds, v86_fs, v86_gs;
};
#endif

/* TSS definition */
struct tss_t     /* TSS for 386+ */
{ 
	uint32_t link,
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
	uint16_t  trace,
		io_map_addr;
	uint8_t  io_map[8192];
};

typedef struct tss_t tss_t;

struct tss0_t  /* TSS for 386+ (no I/O bit map) */
{
	uint32_t link,
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
	uint16_t es, u1,
		cs, u2,
		ss, u3,
		ds, u4,
		fs, u5,
		gs, u6,
		ldts, u7;
	uint16_t trace,
		io_map_addr;
};

typedef union {
	struct {
		uint32_t	max_eax;
		char	vendorid[12];
	} eax_0;
	
	struct {
		uint32_t	stepping	: 4;
		uint32_t	model		: 4;
		uint32_t	family		: 4;
		uint32_t	type		: 2;
		uint32_t	reserved_0	: 18;
		
		uint32_t	reserved_1;
		uint32_t	features; 
		uint32_t	reserved_2;
	} eax_1;
	
	struct {
		uint8_t	call_num;
		uint8_t	cache_descriptors[15];
	} eax_2;

	struct {
		uint32_t	reserved[2];
		uint32_t	serial_number_high;
		uint32_t	serial_number_low;
	} eax_3;

	char		as_chars[16];

	struct {
		uint32_t	eax;
		uint32_t	ebx;
		uint32_t	edx;
		uint32_t	ecx;
	} regs; 
} cpuid_info;

#pragma pack (pop)      /* align structures to default boundary */

struct thread_t;

void    i386SetDescriptor(descriptor_t *item, uint32_t base, uint32_t limit, 
                          uint8_t access, uint8_t attribs);
void    i386SetDescriptorInt(descriptor_int_t *item, uint16_t selector, 
                             uint32_t offset, uint8_t access, uint8_t param_cnt);
uint32_t i386DoCall(void *routine, void *args, uint32_t argbytes);
uint32_t i386DispatchSysCall(uint32_t number, void *args, size_t arg_bytes);
struct thread_t *i386CreateV86Thread(uint32_t entry, uint32_t stack_top, 
                                     unsigned priority, void (*handler)(void),
                                     const wchar_t *name);
bool    i386HandlePageFault(addr_t cr2, bool is_writing);
void    i386MpProbe(void);
void    i386MpInit(void);

extern char scode[];

/*
 * Remote debugging in gdb_stub.c
 */

/* Number of bytes of registers.  */
#define NUMREGBYTES 64
enum i386_regnames {EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
	       PC /* also known as eip */,
	       PS /* also known as eflags */,
	       CS, SS, DS, ES, FS, GS};

extern int i386_registers[NUMREGBYTES / 4];

void	i386InitSerialDebug(void);
void	i386TrapToDebugger(const context_t *ctx);

/*
 * CPUID
 */
extern uint32_t cpuid_ecx, cpuid_edx, cpu_max_level;
uint32_t	i386IdentifyCpu();

static __inline __attribute__((always_inline)) void enable() { __asm__("sti"); }
static __inline __attribute__((always_inline)) void disable() { __asm__("cli"); }
static __inline __attribute__((always_inline)) void invalidate_page(void* page)
{
	__asm__ __volatile__ ("invlpg (%0)": :"r" (page));
}

static __inline __attribute__((always_inline)) void out(uint16_t port, uint8_t value)
{
	__asm__ __volatile__ ("outb %%al, %%dx" : : "d" (port), "a" (value));
}

static __inline __attribute__((always_inline)) void out16(uint16_t port, uint16_t value)
{
	__asm__ __volatile__ ("outw %%ax, %%dx" : : "d" (port), "a" (value));
}

static __inline __attribute__((always_inline)) void out32(uint16_t port, uint32_t value)
{
	__asm__ __volatile__ ("outl %%eax, %%dx" : : "d" (port), "a" (value));
}

static __inline __attribute__((always_inline)) uint8_t in(uint16_t port)
{
	uint8_t ret;
	__asm__ __volatile__ ("inb %%dx,%%al" : "=a" (ret) : "d" (port));
	return ret;
}

static __inline __attribute__((always_inline)) uint16_t in16(uint16_t port)
{
	uint16_t ret;
	__asm__ __volatile__ ("inw %%dx,%%ax" : "=a" (ret) : "d" (port));
	return ret;
}

static __inline __attribute__((always_inline)) uint32_t in32(uint16_t port)
{
	uint32_t ret;
	__asm__ __volatile__ ("inl %%dx,%%eax" : "=a" (ret) : "d" (port));
	return ret;
}

static __inline __attribute__((always_inline)) void halt(uint32_t code)
{
	__asm__("cli ; hlt" : : "a" (code));
}

static __inline __attribute__((always_inline)) uint32_t critb(void)
{
	uint32_t flags;
	__asm__("pushfl\n"
		"pop %0\n"
		"cli" : "=a" (flags));
	return flags;
}

static __inline __attribute__((always_inline)) void crite(uint32_t flags)
{
	__asm__("push %0\n"
		"popfl" : : "g" (flags));
}

static __inline __attribute__((always_inline)) void i386_lmemset32(addr_t addr, uint32_t c, size_t size)
{
	__asm__("mov %0, %%edi\n"
		"mov %1, %%eax\n"
		"mov %2, %%ecx\n"
		"rep stosl" : : "g" (addr), "g" (c), "g" (size));
}

/*! @} */

#endif
