#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/vmm.h>
#include <kernel/proc.h>
#include <stdio.h>

//! Convert an 8086-style seg:ofs address to a linear x86 address
#define LINEAR86(seg, ofs)	(((addr_t) (seg) << 4) + (addr_t) (ofs))

//! The handler called in the event of a GPF in virtual 8086 mode
/*!
 *	If possible, this function emulates the faulting instruction in kernel mode.
 *	\param	ctx	The context structure passed on the stack as a result of the
 *		fault
 *	\return	true if the page fault was handled (and it is safe for the thread
 *		to continue execution), false otherwise.
 */
bool sysV86Fault(context_t* ctx)
{
	byte *ptr;
	word *stack, *ivt;
	bool handled = true;

	ptr = (byte*) LINEAR86(ctx->cs, ctx->eip);
	stack = (word*) LINEAR86(ctx->ss, ctx->esp);
	ivt = (word*) 0;

	switch (ptr[0])
	{
	case 0x6d:	// INS
		wprintf(L"V86: INS encountered at %04x:%04x\n", ctx->cs, ctx->eip);
		//dump_context(ctx);
		handled = false;
		break;
	case 0xcc:	// INT3
		wprintf(L"V86: breakpoint at %04x:%04x\n", ctx->cs, ctx->eip);
		//dump_context(ctx);
		handled = false;
		ctx->eip++;
		break;
	case 0xcd:	// INT n
		stack[0] = (word) (ctx->eip + 2);
		stack[-1] = ctx->cs;
		stack[-2] = (word) ctx->eflags;
		ctx->esp = ((ctx->esp & 0xffff) - 6) & 0xffff;

		ctx->cs = ivt[ptr[1] * 2 + 1];
		ctx->eip = ivt[ptr[1] * 2];
		break;
	case 0xcf:	// IRET
		ctx->eflags = EFLAG_VM | stack[1];
		ctx->cs = stack[2];
		ctx->eip = stack[3];
		ctx->esp = ((ctx->esp & 0xffff) + 6) & 0xffff;
		break;
	case 0xec:	// IN AL,DX
		ctx->regs.eax = (ctx->regs.eax & ~0xff) | in((word) ctx->regs.edx);
		ctx->eip++;
		break;
	case 0xed:	// IN AX,DX
		ctx->regs.eax = (ctx->regs.eax & ~0xffff) | in16((word) ctx->regs.edx);
		ctx->eip++;
		break;
	case 0xee:	// OUT DX,AL
		out(ctx->regs.edx, ctx->regs.eax);
		ctx->eip++;
		break;
	case 0xef:	// OUT DX,AX
		out16(ctx->regs.edx, ctx->regs.eax);
		ctx->eip++;
		break;
	case 0xf4:	// HLT
		wprintf(L"V86: HLT encountered at %04x:%04x\n", ctx->cs, ctx->eip);
		//dump_context(ctx);
		ctx->eip++;
		handled = false;
		break;
	}

	if (!handled)
		wprintf(L"%x %x %x\n", ptr[0], ptr[1], ptr[2]);

	return handled;
}
