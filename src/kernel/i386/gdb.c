/* $Id: gdb.c,v 1.1 2002/01/06 22:46:09 pavlovskii Exp $ */

#include <kernel/arch.h>

/* Got this from http://www.acm.uiuc.edu/sigops/roll_your_own/2.c.html */

#define com1 0x3f8 
#define com2 0x2f8

#define combase com1

void set_debug_traps(void);
void handle_exception(int exceptionVector);
extern descriptor_int_t arch_idt[];

void (*exceptionHook)(int);

void i386InitSerialDebug(void)
{
    out(combase + 3, in(combase + 3) | 0x80);
    out(combase, 12);                           /* 9600 bps, 8-N-1 */
    out(combase+1, 0);
    out(combase + 3, in(combase + 3) & 0x7f);
	set_debug_traps();
}

void i386TrapToDebugger(const context_t *ctx)
{
	i386_registers[EAX] = ctx->regs.eax;
	i386_registers[ECX] = ctx->regs.ecx;
	i386_registers[EDX] = ctx->regs.edx;
	i386_registers[EBX] = ctx->regs.ebx;
	i386_registers[ESP] = ctx->esp;
	i386_registers[EBP] = ctx->regs.ebp;
	i386_registers[ESI] = ctx->regs.esi;
	i386_registers[EDI] = ctx->regs.edi;
	i386_registers[PC] = ctx->eip;
	i386_registers[PS] = ctx->eflags;
	i386_registers[CS] = ctx->cs;
	i386_registers[SS] = ctx->ss;
	i386_registers[DS] = ctx->ds;
	i386_registers[ES] = ctx->es;
	i386_registers[FS] = ctx->fs;
	i386_registers[GS] = ctx->gs;
	handle_exception(ctx->intr);
}

int getDebugChar(void)
{
    while (!(in(combase + 5) & 0x01));
    return in(combase);
}

int putDebugChar(int ch)
{
    while (!(in(combase + 5) & 0x20));
    out(combase, (char) ch);
	return 1;
}

void exceptionHandler(int exc, void *addr)
{
	/*
	 * Page faults are an everyday occurrence -- we don't want to trap into
	 *	the debugger every time one a page is faulted in.
	 * Bochs seems to issue INT1 on every HLT as well.
	 */
	if (exc != 14 &&
		exc != 1)
		i386SetDescriptorInt(arch_idt + exc, 
			KERNEL_FLAT_CODE, 
			(uint32_t) addr, 
			ACS_INT | ACS_DPL_3, 
			ATTR_GRANULARITY | ATTR_BIG);
}

void flush_i_cache(void)
{
   __asm__ __volatile__ ("jmp 1f\n1:");
}

/* 
void *memset(void *ptr, int val, unsigned int len); 

   needs to be defined if it hasn't been already
*/
