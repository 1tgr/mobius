/* $Id: gdb.c,v 1.3 2002/03/13 14:26:24 pavlovskii Exp $ */

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
    out(combase, 12);       /* 9600 bps, 8-N-1 */
    out(combase+1, 0);
    /*out(combase + 3, in(combase + 3) & 0x7f);*/
    out(combase + 3, 3);    /* no parity, 8 bits per character */
    set_debug_traps();
}

int getDebugChar(void)
{
    static char twiddle[] = "|\\-/";
    char *ch = twiddle, str[2];

    while (!(in(combase + 5) & 0x01))
    {
	str[0] = *ch;
	str[1] = '\b';
	_cputs(str, 2);
	ch++;
	if (*ch == '\0')
	    ch = twiddle;
    }

    str[0] = in(combase);
    _cputs(str, 1);
    return str[0];
}

int putDebugChar(int ch)
{
    while (!(in(combase + 5) & 0x20));
	out(combase, (char) ch);
    return 1;
}

void _pputs(const char *str, size_t count)
{
    for (; count > 0; str++, count--)
	putDebugChar(*str);
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

void exceptionHandler(int exc, void *addr)
{
	/*
	 * Page faults are an everyday occurrence -- we don't want to trap into
	 *	the debugger every time a page is faulted in.
	 * Bochs seems to issue INT1 on every HLT as well.
	 */
	if (exc != 14 &&
		exc != 1 &&
		exc != 8)
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
