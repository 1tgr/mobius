/* $Id: i386.c,v 1.14 2002/02/22 16:51:35 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>
#include <kernel/thread.h>
#include <kernel/sched.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/driver.h>
#include <kernel/debug.h>

#include <stdio.h>
#include <os/syscall.h>

extern unsigned sc_need_schedule;
extern irq_t *irq_first[16], *irq_last[16];

extern tss_t arch_tss;
extern descriptor_t arch_gdt[];
extern addr_t kernel_pagedir[];

void TextSwitchToKernel(void);

/*! \brief	Sets up a a code or data entry in the IA32 GDT or LDT
 *
 *	\param	item	Pointer to a descriptor table entry
 *	\param	base	Address of the base used for the descriptor
 *	\param	limit	Size of the end of the code or data referenced by the 
 *		descriptor, minus one
 *	\param	access	Access permissions applied to the descriptor
 *	\param	attribs	Attributes applied to the descriptor
 */
void i386SetDescriptor(descriptor_t *item, uint32_t base, uint32_t limit, 
					   uint8_t access, uint8_t attribs)
{
	item->base_l = base & 0xFFFF;
	item->base_m = (base >> 16) & 0xFF;
	item->base_h = base >> 24;
	item->limit = limit & 0xFFFF;
	item->attribs = attribs | ((limit >> 16) & 0x0F);
	item->access = access;
}

/*! \brief	Sets up an interrupt, task or call gate in the IA32 IDT
 *
 *	\param	item	Pointer to an IDT entry
 *	\param	selector	The CS value referred to by the gate
 *	\param	offset	The EIP value referred to by the gate
 *	\param	access	Access permissions applied to the gate
 *	\param	attribs	Attributes applied to the gate
 */
void i386SetDescriptorInt(descriptor_int_t *item, uint16_t selector, 
						  uint32_t offset, uint8_t access, uint8_t param_cnt)
{
	item->selector = selector;
	item->offset_l = offset & 0xFFFF;
	item->offset_h = offset >> 16;
	item->access = access;
	item->param_cnt = param_cnt;
}

/*! \brief Sets up the PC's programmable timer to issue interrupts at HZ 
 *		(100 Hz) instead of the default ~18.2 Hz.
 *	\param	hz	The required clock frequency, in hertz
 */
void i386PitInit(unsigned hz)
{
	unsigned short foo = (3579545L / 3) / hz;

	/* reprogram the 8253 timer chip to run at 'HZ', instead of 18 Hz */
	out(0x43, 0x36);	/* channel 0, LSB/MSB, mode 3, binary */
	out(0x40, foo & 0xFF);	/* LSB */
	out(0x40, foo >> 8);	/* MSB */
}

/*! \brief	Sets up the PC's programmable interrupt controllers to issue IRQs 
 *		on interrupts master_vector...slave_vector+7.
 *	\param	master_vector	The interrupt vector the master PIC will use
 *	\param	slave_vector	The interrupt vector the slave PIC will use
 */
void i386PicInit(uint8_t master_vector, uint8_t slave_vector)
{
	out(PORT_8259M, 0x11);					/* start 8259 initialization */
	out(PORT_8259S, 0x11);
	out(PORT_8259M + 1, master_vector); 	/* master base interrupt vector */
	out(PORT_8259S + 1, slave_vector);		/* slave base interrupt vector */
	out(PORT_8259M + 1, 1<<2);				/* bitmask for cascade on IRQ2 */
	out(PORT_8259S + 1, 2); 				/* cascade on IRQ2 */
	out(PORT_8259M + 1, 1); 				/* finish 8259 initialization */
	out(PORT_8259S + 1, 1);

	out(PORT_8259M + 1, 0); 				/* enable all IRQs on master */
	out(PORT_8259S + 1, 0); 				/* enable all IRQs on slave */
}

void i386_lpoke32(addr_t off, uint32_t value)
{
	__asm __volatile__("movl %0,%%gs:(%1)" : : "r" (value), "r" (off));
}

uint32_t i386_lpeek32(addr_t off)
{
	uint32_t value;
	__asm __volatile__("movl %%gs:(%1),%0" : "=a" (value): "r" (off));
	return value;
}

uint32_t i386Isr(context_t ctx)
{
	thread_t *old_current;

	old_current = current;
	current->ctx_last = &ctx;
	
	if (ctx.error == (uint32_t) -1)
	{
		irq_t *irq;
		unsigned i;

		((uint16_t*) PHYSICAL(0xb8000))[1] = ~((uint16_t*) PHYSICAL(0xb8000))[1];

		ArchMaskIrq(0, 1 << ctx.intr);
		
		if (ctx.intr == 0)
		{
			sc_uptime += SCHED_QUANTUM;
			ScNeedSchedule(true);
		}
		
		irq = irq_first[ctx.intr];
		i = 0;
		while (irq)
		{
			assert(irq->dev->vtbl->isr);
			if (irq->dev->vtbl->isr(irq->dev, ctx.intr))
				break;
			irq = irq->next;
			i++;
			if (i > 6)
				assert(false && "Too many interrupt handlers");
		}

		out(PORT_8259M, EOI);
		if (ctx.intr >= 8)
			out(PORT_8259S, EOI);

		ArchMaskIrq(1 << ctx.intr, 0);
	}
	else
	{
		addr_t cr2;
		bool handled;
		
		__asm__("mov %%cr2, %0"
			: "=r" (cr2));

		handled = false;
		if (ctx.intr == 1)
		{
			uint32_t dr6;
			__asm__("mov %%dr6, %0" : "=r" (dr6));
			if (dr6 & DR6_BS)
				wprintf(L"debug: %lx:%08lx\n", ctx.cs, ctx.eip);
			handled = true;
		}
		else if (ctx.intr == 14 &&
			(ctx.error & PF_PROTECTED) == 0)
		{
			handled = ProcPageFault(current->process, cr2);
			if (!handled &&
				cr2 >= 0x80000000 &&
				kernel_pagedir[PAGE_DIRENT(cr2)] &&
				(*ADDR_TO_PDE(cr2) & PRIV_PRES) == 0)
			{
				cr2 = PAGE_ALIGN(cr2);
				wprintf(L"Mapping kernel page at %x for process %u...", 
					cr2, current->process->id);
				*ADDR_TO_PDE(cr2) = kernel_pagedir[PAGE_DIRENT(cr2)];
				invalidate_page((void*) cr2);
				wprintf(L"done\n");
				handled = true;
			}
		}
		else if (ctx.intr == 0x30)
		{
			i386DispatchSysCall(&ctx);
			handled = true;
		}

		if (!handled)
		{
			/*uint8_t *eip;*/

			TextSwitchToKernel();

			wprintf(L"Thread %s/%u: Interrupt %ld at %lx:%08lx: %08lx\n", 
				current->process->exe, current->id, 
				ctx.intr, ctx.cs, ctx.eip, cr2);
			
			if (ctx.intr == 14)
			{
				wprintf(L"Page fault: ");

				if (ctx.error & PF_PROTECTED)
					wprintf(L"protection violation ");
				else
					wprintf(L"page not present ");

				if (ctx.error & PF_WRITE)
					wprintf(L"writing ");
				else
					wprintf(L"reading ");

				if (ctx.error & PF_USER)
					wprintf(L"in user mode\n");
				else
					wprintf(L"in kernel mode\n");
			}

			DbgDumpStack(current->process, ctx.regs.ebp);
			ArchDbgDumpContext(&ctx);
			/*DbgDumpBuffer((char*) ctx.eip - 8, 16);*/

			if (current->process == &proc_idle)
				i386TrapToDebugger(&ctx);
			else
				ProcExitProcess(-ctx.intr);
		}
	}
	
	current->ctx_last = ctx.ctx_prev;
	while (true)
	{
		if (sc_need_schedule)
			ScSchedule();
		
		if (old_current != current)
		{
			old_current->kernel_esp = ctx.kernel_esp;
			if (ArchAttachToThread(current, old_current != current))
				return current->kernel_esp;
			else
				ScNeedSchedule(true);
		}
		else
			return ctx.kernel_esp;
	}
}

void i386DoubleFault(uint32_t error, uint32_t eip, uint32_t cs, uint32_t eflags)
{
	/*
	 * A double fault has occurred so we're running under the double fault TSS.
	 * The context of the faulting thread is saved in the general-purpose TSS.
	 * (Double faults usually mean we've messed up one of the kernel stacks.)
	 */
	_cputws(L"Double fault ", 13);
	wprintf(L"at %x: ", arch_tss.eip);
	wprintf(L"esp0 = %x\n", arch_tss.esp0);
	_cputws(L"System halted\n", 14);
	__asm__("cli ; hlt" : : "a" (arch_tss.eip));
}
