/* $Id: gdb.c,v 1.5 2002/03/27 22:06:32 pavlovskii Exp $ */

#include <kernel/arch.h>
#include <kernel/profile.h>
#include <wchar.h>

/* Got this from http://www.acm.uiuc.edu/sigops/roll_your_own/2.c.html */

void set_debug_traps(void);
void handle_exception(int exceptionVector);
extern descriptor_int_t arch_idt[];

void (*exceptionHook)(int);

uint16_t dbg_combase;
bool dbg_hasgdb;

void i386InitSerialDebug(void)
{
    dbg_combase = wcstol(ProGetString(L"KernelDebug", L"Port", L"0"), NULL, 0);
    dbg_hasgdb = ProGetBoolean(L"KernelDebug", L"SyncGdb", false);
    if (dbg_combase != 0)
    {
	out(dbg_combase + 3, in(dbg_combase + 3) | 0x80);
	out(dbg_combase, 12);       /* 9600 bps, 8-N-1 */
	out(dbg_combase+1, 0);
	/*out(dbg_combase + 3, in(dbg_combase + 3) & 0x7f);*/
	out(dbg_combase + 3, 3);    /* no parity, 8 bits per character */
	wprintf(L"i386InitSerialDebug: kernel debugger running on port %x\n", dbg_combase);
	
	if (dbg_hasgdb)
        {
            set_debug_traps();
	    __asm__("int3");
        }
    }
    else
	wprintf(L"i386InitSerialDebug: kernel debugger not running\n");
}

int getDebugChar(void)
{
    static char twiddle[] = "|\\-/";
    char *ch = twiddle, str[2];

    while (!(in(dbg_combase + 5) & 0x01))
    {
	/*str[0] = *ch;
	str[1] = '\b';
	_cputs(str, 2);
	ch++;
	if (*ch == '\0')
	    ch = twiddle;*/
    }

    /*str[0] = in(dbg_combase);
    _cputs(str, 1);
    return str[0];*/
    return in(dbg_combase);
}

int putDebugChar(int ch)
{
    while (!(in(dbg_combase + 5) & 0x20));
	out(dbg_combase, (char) ch);
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
	if (exc != 14 &&    /* page fault (normal) */
	    exc != 13 &&    /* GPF (normal in V86 mode) */
	    exc != 8 &&	    /* double fault (so abnormal we just crash) */
	    exc != 1)	    /* debug (seems to get issued on every HLT) */
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
