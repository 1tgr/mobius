#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/wrappers.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/ramdisk.h>
#include <kernel/driver.h>
#include <kernel/serial.h>
#include <kernel/sys.h>
#include <kernel/fs.h>

//! Frequency of the timer interrupt
#define HZ					1
//! Number of milliseconds between timer interrupts
#define MS					(1000 / HZ)

#define ERR_PAGE_NOT_PRESENT		0
#define ERR_PROTECTION_VIOLATION	1
#define ERR_READ					0
#define ERR_WRITE					2
#define ERR_SUPERVISOR				0
#define ERR_USER					4

extern byte scode[], bdata[], edata[], ebss[];
extern descriptor_t _gdt[];
extern gdtr_t _gdtr;
extern dword kernel_pagedir[];
extern byte code86[], code86_end[];

//byte exception_stack[PAGE_SIZE];

//! The kernel's errno
//int kernel_errno;
//! The system interrupt descriptor table
descriptor_int_t idt[countof(_wrappers)];
//! The system IDT register
idtr_t idtr;
//! The TSS used to switch between user and kernel modes
tss_t tss;
//! The address of the start of the kernel's heap
dword kernel_sbrk = 0xF0000000;
//! Information on each IRQ
irq_t *irq_first[16], *irq_last[16];
//! The number of milliseconds elapsed since the kernel started
volatile dword uptime;
bool (CDECL *dbgAttach)(thread_t*, context_t*, addr_t);

handle_t hnd_idle = { NULL, 0, (byte) -1, NULL, NULL, 0 };
//! process_t structure for the idle process (plus kernel startup code)
process_t proc_idle;
//! process_info_t structure for the idle process
process_info_t proc_idle_info;
module_info_t kernel_info = { 0x100, NULL, 0xc0000000, 0x10000000, L"kernel.exe" };

//! thread_t structure for the idle thread (plus kernel startup code)
thread_t thr_idle =
{
	&proc_idle,	//! process
	0,		//! kernel_esp
	-1,		//! id
	NULL,	//! prev
	NULL,	//! next
	NULL,	//! kernel_stack
	NULL,	//! user_stack
	//NULL,	//! v86_stack
	0,		//! suspend
	THREAD_RUNNING	//! state
};
//! thread_info_t structure for the idle thread
thread_info_t thr_idle_info;

int *__errno()
{
	return &current->info->last_error;
}

//! Allocates more memory for the kernel heap
/*!
 *	The kernel, unable to use the libc morecore() routine, uses this routine to 
 *		allocate and map more heap pages.
 *	\param	nu	Size, in multiples of sizeof(BlockHeader), of the block to 
 *		allocate. This is rounded to PAGE_SIZE automatically, and increased
 *		to four pages if smaller.
 *	\return	A pointer to the start of the block allocated. This block is also
 *		added to the freed memory list.
 */
BlockHeader* morecore(size_t nu)
{
	byte *cp;
	BlockHeader *up;
	dword dwBytes, allocated;
	extern BlockHeader* freep;
	addr_t phys;

	dwBytes = ((nu * sizeof(BlockHeader)) + PAGE_SIZE) & -PAGE_SIZE;
	if (dwBytes < PAGE_SIZE * 4)
		dwBytes = PAGE_SIZE * 4;

	wprintf(L"morecore: need %u bytes: ", dwBytes);

	cp = (byte*) kernel_sbrk;
	for (allocated = 0; allocated < dwBytes; allocated += PAGE_SIZE)
	{
		phys = memAlloc();

		if (phys == NULL)
			return NULL;

		memMap(kernel_pagedir, kernel_sbrk, phys, 1, 
			PRIV_KERN | PRIV_WR | PRIV_PRES);
		invalidate_page((void*) kernel_sbrk);
		kernel_sbrk += PAGE_SIZE;
		wprintf(L"%x ", phys);
	}
	
	wprintf(L"at %p\n", cp);
	nu = dwBytes / sizeof(BlockHeader);
	up = (BlockHeader*) cp;
	up->s.size = nu;
	free((void*) (up + 1));
	
	return freep;
}

//! Dumps a context structure on the screen
void dump_context(thread_t* thr, const context_t* ctx)
{
	wprintf(L" EAX %08x\t EBX %08x\t ECX %08x\t EDX %08x\n",
		ctx->regs.eax, ctx->regs.ebx, ctx->regs.ecx, ctx->regs.edx);
	wprintf(L" ESI %08x\t EDI %08x\t EBP %08x\n",
		ctx->regs.esi, ctx->regs.edi, ctx->regs.ebp);
	wprintf(L"  CS %08x\t  DS %08x\t  ES %08x\t  FS %08x\n",
		ctx->cs, ctx->ds, ctx->es, ctx->fs);
	wprintf(L"  GS %08x\t  SS %08x\n",
		ctx->gs, ctx->ss);
	wprintf(L" EIP %08x\teflg %08x\tESP0 %08x\tESP3 %08x\n", 
		ctx->eip, ctx->eflags, ctx->kernel_esp, ctx->esp);
}

//! The main page fault handler
/*!
 *	\param	fault_addr	The address referenced at the time of the fault, from
 *		the CR3 register on the i386
 *	\param	ctx	The context structure passed on the stack as a result of the
 *		fault
 *	\return	true if the page fault was handled (and it is safe for the thread
 *		to continue execution), false otherwise.
 */
bool sysPageFault(addr_t fault_addr, context_t* ctx)
{
	process_t* proc;
	bool handled = false;
	module_t *mod;
	dword oldflags;

	//wprintf(L"PF(%x @ %x)\n", fault_addr, ctx->eip);
	if ((ctx->error & ERR_PROTECTION_VIOLATION) == 0)
	{
		if ((fault_addr & -PAGE_SIZE) == NULL)
			return false;

		if (fault_addr == 0xcdcdcdcd)
		{
			_cputws(L"Uninitialised pointer access\n");
			return false;
		}
		
		if (ramPageFault(fault_addr))
			return true;

		if (current && current->process)
		{
			proc = current->process;
		
			if (procPageFault(current->process, fault_addr))
				return true;
			
			for (mod = proc->mod_first; mod; mod = mod->next)
			{
				oldflags = ctx->eflags;
				handled = pePageFault(proc, mod, fault_addr);
				
				if (ctx->cs == 0x2b && 
					ctx->eflags & EFLAG_TF && 
					(oldflags & EFLAG_TF) == 0)
					wprintf(L"%s: %04x:%08x TF is set and was not set before\n", 
						mod->name, ctx->cs, ctx->eip);

				if (handled)
					return true;
			}
		}
	}

	return handled;
}

//! Converts an integer to a string.
/*!
 *	\param	value	The integer to be converted
 *	\param	string	A pointer to the buffer which will receive the string. This
 *		must be large enough to accept the string (e.g. a maximum of eight 
 *		characters of hexadecimal)
 *	\param	radix	Specifies the numberic base to convert to
 */
void itow(dword value, wchar_t* string, int radix)
{
	wchar_t buf[9], *ch = buf + countof(buf) - 1;
	byte n;
	
	*ch = 0;
	do
	{
		ch--;
		n = value % radix;
		if (n > 9)
			*ch = 'a' + n - 10;
		else
			*ch = '0' + n;
		value /= radix;
	} while (value);

	while (*ch)
		*string++ = *ch++;

	*string = 0;
}

bool dbgInvoke(thread_t* thr, context_t* ctx, addr_t addr)
{
	if (dbgAttach)
		return dbgAttach(thr, ctx, addr);
	else
		return false;
}

//! The function called by isr() in the event of an exception (an interrupt
//!		within the 0-31 range reserved by Intel)
/*!
 * Passes the exception onto either sysPageFault() or sysV86Fault(). If the 
 *		exception could not be handled, this function passes it to user mode,
 *		if possible; otherwise it terminates the process or locks the system
 *		(in the case of an error in the idle thread).
 *	\param	thr	The thread that caused the exception. The function runs in the
 *		context of this thread. If an exception occurs before the kernel 
 *		starts user mode, this will be &thr_idle, unless an error has occurred
 *		which causes current to become NULL.
 *	\param	ctx	The context structure passed on the stack as a result of the
 *		fault
 */
void exception(thread_t* thr, context_t* ctx, dword code, dword address)
{
	static const wchar_t *msg1[] =
	{
		L"divide error", L"debug exception", L"NMI", L"INT3",
		L"INTO", L"BOUND overrange", L"invalid opcode", L"no coprocessor",
		L"double fault", L"coprocessor segment overrun",
			L"bad TSS", L"segment not present",
		L"stack fault", L"GPF", L"page fault", L"??",
		L"coprocessor error", L"alignment check", L"??", L"??",
		L"??", L"??", L"??", L"??",
		L"??", L"??", L"??", L"??",
		L"??", L"??", L"??", L"??"
	}, *msg2[] =
	{
		L"missing DLL", L"missing import"
	};

	static bool lock = false;
	wchar_t str[9];
	const wchar_t* msg;
	dword exc;
	int i;
	process_t* proc;
	byte *ptr;

	if (thr)
	{
		if (ctx == NULL)
			ctx = thrContext(thr);

		proc = thr->process;
	}
	else
		proc = NULL;
	
	if (!lock)
	{
		lock = true;
		for (ptr = 0; ptr < (byte*) 320; ptr++)
			0xa0000[ptr] = (byte) (dword) ptr;

		if (thr)
		{
			itow(ctx->eip, str, 16);

			/*unk = sysOpen(L"devices/screen");
			if (unk)
			{
				IStream* strm;
	
				if (SUCCEEDED(unk->vtbl->QueryInterface(unk, &IID_IStream, (void**) &strm)))
				{
					strm->vtbl->Write(strm, str, wcslen(str) * sizeof(wchar_t));
					strm->vtbl->Write(strm, L": ", 2 * sizeof(wchar_t));
					itow(fault_addr, str, 16);
					wcscat(str, L"\n");
					strm->vtbl->Write(strm, str, wcslen(str) * sizeof(wchar_t));
					strm->vtbl->Release(strm);
				}

				unk->vtbl->Release(unk);
			}*/

			exc = 0;
			
			/*if (ctx->cs != 0x18 && code != 3)
			{
				for (i = 0; i < proc->num_modules; i++)
				{
					exc = (dword) proc->modules[i]->vtbl->GetProcAddress(
						proc->modules[i], "sysException");
					if (exc)
					{
						dword frame[] = { code, address, ctx->eip };
						thrCall(thr, (void*) exc, frame, sizeof(frame));
						//asm("mov %0, %%eax ; hlt" : : "r" (ctx.eip));
						break;
					}
				}
			}*/

			if (!exc && !dbgInvoke(thr, ctx, address))
			{
				if (ctx->cs == 0x18)
					_cputws(L"Kernel: ");

				if (code < countof(msg1))
					msg = msg1[code];
				else if (code >= 0x80000000 &&
					code - 0x80000000 < countof(msg2))
					msg = msg2[code - 0x80000000];
				else
					msg = L"exception";

				wprintf(L"\xbb\xbb unhandled %s (%u) at %04x:%08x (%08x) in thread %u\n",
					msg, code, ctx->cs, ctx->eip, address, thr->id);

				dump_context(current, ctx);
				if (current == &thr_idle)
				{
					asm("mov %0, %%ebx" : : "r" (address));
					halt(ctx->eip);
				}
				else
				{
					_cputws(L"Terminate process\n");
					procTerminate(proc);
				}
			}
		}
		else
		{
			for (i = 0; i < countof(str) - 1; i++)
				out(0xfff0, str[i]);

			halt(ctx->eip);
		}
		
		lock = false;
	}
	else
		halt(ctx->eip);
}

//! Generic interrupt handler
/*!
 *	Takes the appropriate action on an interrupt:
 *	+ Calls thrSchedule() on the timer IRQ
 *	+ Calls IRQ handlers, if available, and signals EOI to the PIC
 *	+ Calls exception() for exceptions (interrupts 0-31)
  *	\param	ctx	Interrupt and context information passed directly on the stack
 *		by the individual interrupt handlers
 */
dword isr(context_t ctx)
{
	addr_t fault_addr;
	bool handled = false;
	thread_t* old_current = current;
	irq_t *irq;

	//current->kernel_esp = ctx.kernel_esp;
	
	//if (ctx.intr)
		//wprintf(L"%02d %x\t", ctx.intr, &ctx);

	if (ctx.error == (dword) -1)
	{
		if (ctx.intr == 0)
		{
			uptime += MS;
			if (!current->suspend)
				thrSchedule();
		}
		
		out(PORT_8259M, EOI);
		if (ctx.intr >= 8)
			out(PORT_8259S, EOI);

		irq = irq_first[ctx.intr];
		while (irq)
		{
			if (irq->dev)
			{
				request_t req;
				req.code = DEV_ISR;
				req.result = 0;
				req.params.isr.irq = ctx.intr;
				req.event = NULL;
				if (irq->dev->request(irq->dev, &req))
					break;
			}

			irq = irq->next;
		}
	}
	else
	{
		asm("mov %%cr2,%0": "=r" (fault_addr));
		
		//enable();
		if (ctx.intr == EXCEPTION_GPF && 
			(ctx.eflags & EFLAG_VM) && 
			current->v86handler)
			handled = current->v86handler(&ctx);
		
		if (ctx.intr == EXCEPTION_PAGE_FAULT)
			handled = sysPageFault(fault_addr, &ctx);
		
		if (!handled)
		{
			/*if (ctx.intr == EXCEPTION_DEBUG)
				wprintf(L"%08x ", ctx.eip);
			else*/ if (ctx.intr == 0x30)
				syscall(&ctx);
			else if (ctx.intr < 0x10)
				exception(current, &ctx, ctx.intr, fault_addr);
			else
				wprintf(L"interrupt %d\n", ctx.intr);
		}
	}

	//if (ctx.intr)
		//wprintf(L"FS = %x GS = %x\n", ctx.fs, ctx.gs);

	/*{
		wchar_t str[9];
		int i;

		itow(ctx.kernel_esp, str, 16);
		for (i = 0; str[i]; i++)
			i386_lpoke16(0xb8000 + (81 - countof(str) + i) * 2,
				0x5300 | str[i]);

		itow(ctx.fs, str, 16);
		for (i = 0; str[i]; i++)
			i386_lpoke16(0xb8000 + (80 + 81 - countof(str) + i) * 2,
				0x5300 | str[i]);
	}*/

	//return current->kernel_esp;

	if (old_current != current)
	{
		context_t *old, *new;
		old_current->kernel_esp = ctx.kernel_esp;

		if (current->process != old_current->process)
			asm("mov %0, %%cr3" : : "r" (current->process->page_dir));

		old = thrContext(old_current);
		new = thrContext(current);

		/*if (old->cs == 0x2b || new->cs == 0x2b)
		{
			wprintf(L"[%d] old = %x, %x(%x) new = %x, %x(%x)\n", 
				current->id,
				old->regs.ebp, old->esp, old->eip, 
				new->regs.ebp, new->esp, new->eip);
			//dump_context(old_current, old);
			//dump_context(current, new);
		}*/

		//tss.esp0 = current->kernel_esp + sizeof(context_t);
		tss.esp0 = (dword) (new + 1);
		return current->kernel_esp;
	}
	else
		return ctx.kernel_esp;
}

//! Sets up the PC's programmable timer to issue interrupts at HZ (100 Hz) 
//!		instead of the default ~18.2 Hz.
/*!
 *	\param	hz	The required clock frequency, in hertz
 */
void INIT_CODE pitInit(word hz)
{
	unsigned short foo = (3579545L / 3) / hz;

	/* reprogram the 8253 timer chip to run at 'HZ', instead of 18 Hz */
	out(0x43, 0x36);	/* channel 0, LSB/MSB, mode 3, binary */
	out(0x40, foo & 0xFF);	/* LSB */
	out(0x40, foo >> 8);	/* MSB */
}

//! Sets up the PC's programmable interrupt controllers to issue IRQs on 
//!		interrupts master_vector...slave_vector+7.
/*!
 *	\param	master_vector	The interrupt vector the master PIC will use
 *	\param	slave_vector	The interrupt vector the slave PIC will use
 */
void INIT_CODE picInit(byte master_vector, byte slave_vector)
{
	out(PORT_8259M, 0x11);                  /* start 8259 initialization */
	out(PORT_8259S, 0x11);
	out(PORT_8259M + 1, master_vector);     /* master base interrupt vector */
	out(PORT_8259S + 1, slave_vector);      /* slave base interrupt vector */
	out(PORT_8259M + 1, 1<<2);              /* bitmask for cascade on IRQ2 */
	out(PORT_8259S + 1, 2);                 /* cascade on IRQ2 */
	out(PORT_8259M + 1, 1);                 /* finish 8259 initialization */
	out(PORT_8259S + 1, 1);

	out(PORT_8259M + 1, 0);                 /* enable all IRQs on master */
	out(PORT_8259S + 1, 0);                 /* enable all IRQs on slave */
}

//! Called by the assert() macro when an assertion fails.
/*!
 *	\param	file	The source file in which an assertion failed
 *	\param	line	The line on which the assertion failed
 *	\param	exp		The expression which failed
 */
void assert_fail(const char* file, int line, const wchar_t* exp)
{
	wprintf(L"** Kernel assertion failed!\n"
		L"\t%S, line %u: %s\n",
		file, line, exp);

	if (current)
	{
		wprintf(L"Thread %d\n", current->id);
		exception(current, thrContext(current), EXCEPTION_ASSERT_FAILED, 0);
	}
	else
	{
		_cputws(L"System halted.\n");
		halt(0);
	}
}

bool keDiscardSection(const void* base, const char* name)
{
	const IMAGE_DOS_HEADER* doshead = (const IMAGE_DOS_HEADER*) base;
	const IMAGE_PE_HEADERS* header;
	const IMAGE_SECTION_HEADER* sections;
	byte *virt;
	int i;
	addr_t phys;

	if (doshead->e_magic != IMAGE_DOS_SIGNATURE)
		return false;

	header = (const IMAGE_PE_HEADERS*) ((const byte*) doshead + doshead->e_lfanew);
	if (header->Signature != IMAGE_NT_SIGNATURE)
		return false;

	sections = IMAGE_FIRST_SECTION(header);
	for (i = 0; i < header->FileHeader.NumberOfSections; i++)
	{
		if (strncmp(sections[i].Name, name, countof(sections[i].Name)) == 0)
		{
			wprintf(L"Discarding %x%S: %d bytes\n", 
				base, sections[i].Name, sections[i].Misc.VirtualSize);
			virt = (byte*) base + sections[i].VirtualAddress;
			phys = memTranslate(kernel_pagedir, virt) & -PAGE_SIZE;

			wprintf(L"Mapped %x to %x\n", virt, phys);
			//memset(virt, 0xcc, sections[i].Misc.VirtualSize);
#if 0
			memFree(phys, sections[i].Misc.VirtualSize);
			memMap(kernel_pagedir, (addr_t) virt, NULL, 
				sections[i].Misc.VirtualSize / PAGE_SIZE, 0);
#endif

			return true;
		}
	}

	return false;
}

void KernelThread(dword num)
{
	word *vmem = (word*) 0xb8000;
	int b = current->id;

	while (true)
	{
		vmem[b]++;
		asm("hlt");
		//sysYield();
	}
}

static const wchar_t* INIT_DATA drivers[] =
{
	L"kdebug.dll",
	L"isa.drv",
	L"pci.drv",
};

void conInit(int mode);
void devInit();
bool vfsInit();

//! The real entry point for the kernel, called by _main() once a GDT is set up
/*!
 *	This is really the only function which executes in the kernel: it performs
 *		system initialisation, including setting up the kernel and basic 
 *		hardware and loading device drivers; then loads the user shell
 *		and enters an idle loop.
 *
 *	This function forms the idle thread's flow of control.
 *	\return	Nothing, really: this function enters an infinite loop eventually.
 */
int main()
{
	int i;
	module_t* mod;
	bool (STDCALL *drvInit) (driver_t*);
	bool success;
	driver_t drv;
	process_t* shell;
	
	conInit(1);
	
	/* 
	 * _cputws_blue does not map Unicode characters: we are using the VGA 
	 *	card's DOS 437 font, which does not include a copyright symbol.
	 *	Similarly, we use ASCII 148 for the o-umlaut. 
	 */
	_cputws_check(L"Loading The M”bius\t\tCopyright (C) 2001 Tim Robinson\n");
	
	/* Create the physical page map; set up the kernel page directory */
	_cputws_check(L"\x10 Initial setup\r");
	memInit();
	malloc(0);
	
	/* Set up the TSS */
	//thr_idle.kernel_stack = malloc(PAGE_SIZE);
	thr_idle.kernel_stack = (void*) 0xdeadbeef;
	thr_idle.kernel_esp = (dword) thr_idle.kernel_stack + PAGE_SIZE;

	memset(&tss, 0, sizeof(tss) - sizeof(tss.io_map));

	i386_lmemset32((addr_t) tss.io_map, 0xffffffff, sizeof(tss.io_map));

	i386_set_descriptor(_gdt + 7, (addr_t) &tss - (addr_t) scode + _sysinfo.kernel_phys, 
		sizeof(tss_t), ACS_TSS, ATTR_BIG);
	tss.io_map_addr = (byte*) tss.io_map - (byte*) &tss;
	tss.ss0 = 0x20;

	/*
	 * The value of tss.esp makes no difference until the first task switch is made:
	 * - initially, CPU is in ring 0
	 * - first timer IRQ occurs -- processor executes timer interrupt
	 * - no PL switch so tss.esp0 is ignored
	 * - current thread's esp is ignored
	 * - when a task switch ocurs, tss.esp0 is updated
	 */
	tss.esp0 = thr_idle.kernel_esp;

	asm("ltr %0": : "r" ((word) 0x38));
	
	/* Change the RTC speed to HZ -- default 100 Hz */
	pitInit(HZ);

	/* Re-map IRQs to fit with our IDT */
	picInit(0x20, 0x28);

	/* Set up the IDT */
	idtr.base = (addr_t) idt;
	idtr.limit = sizeof(idt) - 1;

	for (i = 0; i < countof(_wrappers); i++)
		i386_set_descriptor_int(idt + i, 0x18, (addr_t) _wrappers[i], ACS_INT | ACS_DPL_0, 
			ATTR_GRANULARITY | ATTR_BIG);

	/* int 3: breakpoint; if this is DPL0 it generates a GPF */
	i386_set_descriptor_int(idt + 3, 0x18, (addr_t) _wrappers[3], ACS_INT | ACS_DPL_3, 
			ATTR_GRANULARITY | ATTR_BIG);
	/* int 30: syscall */
	i386_set_descriptor_int(idt + 0x30, 0x18, (addr_t) _wrappers[0x30], ACS_INT | ACS_DPL_3, 
			ATTR_GRANULARITY | ATTR_BIG);

	current = thr_first = thr_last = &thr_idle;
	asm("lidt	(_idtr)");

#if 0
	//_cputws_check(L" \n\x10 Serial port\r");

	/* Open and test the serial port */
	ioOpenPort(0x3f8, 4);
	ioSetBaud(NULL, 9600);
	ioSetControl(NULL, BITS_8 | NO_PARITY | STOP_1);
	ioSetHandShake(NULL, 0);

	ioWriteBuffer(NULL, "Hello from The M”bius!\n", 23);
#endif

	//_cputws_check(L" \n\x10 Ramdisk\r");
	/* Initialise the ramdisk */
	ramInit();

	//_cputws_check(L" \n\x10 sti\r");

	/* Set up pointers for the (system) idle process */
	proc_idle.page_dir = 
		(dword*) ((addr_t) kernel_pagedir - (addr_t) scode + _sysinfo.kernel_phys);
	proc_idle.vmm_end = _sysinfo.memory_top;
	proc_idle.stack_end = 0;
	proc_idle.info = &proc_idle_info;
	proc_idle.module_last = &kernel_info;
	proc_idle_info.id = 0;
	proc_idle_info.root = NULL;
	proc_idle_info.module_first = &kernel_info;
	wcscpy(proc_idle_info.cwd, L"/boot");

	/* Set up pointers for the idle thread */
	thr_idle.info = &thr_idle_info;
	thr_idle.info->info = thr_idle.info;
	thr_idle.info->process = &proc_idle_info;

	/* Ensure the idle thread has a valid fs */
	i386_set_descriptor(_gdt + 8, (addr_t) thr_idle.info, 1, 
		ACS_DATA | ACS_DPL_0, ATTR_BIG | ATTR_GRANULARITY);
	asm("mov $0x40,%%eax ;"
		"mov %%eax,%%fs ;": : : "eax");
	
	/* Enable interrupts */
	thr_idle.suspend++;
	enable();
	thr_idle.suspend--;

	sysInit();
	devInit();
	
	wprintf(L"%dMB memory was detected\n"
		L"\tKernel is %dKB at %x, ramdisk is %dKB at %x\n",
		_sysinfo.memory_top / 1048576, 
		_sysinfo.kernel_data / 1024, 
		_sysinfo.kernel_phys,
		_sysinfo.ramdisk_size / 1024,
		_sysinfo.ramdisk_phys);

	/* Create a kernel link for devices */
	sysMount(L"devices", NULL);
	sysOpen(L"devices");
	
	peLoadMemory(&proc_idle, L"kernel.exe", (void*) 0xc0000000, 0);
	
	/* Load the kernel drivers */
	for (i = 0; i < countof(drivers); i++)
	{
		_cputws_check(L" \n\x10 ");
		_cputws_check(drivers[i]);
		_cputws_check(L"\r");

		success = false;

		mod = peLoad(&proc_idle, drivers[i], 0);
		if (mod)
		{
			const void* da;

			drvInit = (void*) mod->entry;
			if (drvInit)
				success = drvInit(&drv);
			else
				success = true;
			
			da = (const void*) peGetExport(mod, "dbgAttach");
			if (da)
			{
				dbgAttach = da;
				success = true;
			}

			//keDiscardSection(mod->vtbl->GetBase(mod), ".init");
		}

		if (!success)
			_cputws_check(L"!");
	}

	vfsInit();

	/*{
		file_t* fd;
		byte buffer[11];
		size_t length = 10;

		fd = fsOpen(L"/boot/isa.cfg");
		wprintf(L"Opened %p\n", fd);
		buffer[10] = 0;
		fsRead(fd, buffer, &length);
		wprintf(L"Buffer = %S\n", buffer);
		fsClose(fd);
	}*/

	_cputws_check(L" \n\x10 Ready\r");
	//asm("int3");
	//conSetMode(modeConsole);
	//_cputws(L"\x1b[2J");

	/* Give the idle process a root directory */
	//proc_idle.info->root = sysOpen(L"root");

	//keDiscardSection(scode, ".init");

	/* Kernel initialisation is finished -- display system information for confirmation */
	wprintf(L"The kernel is ready!\n");
	//asm("mov $4, %eax ; int $0x30");
	
	{
		thread_t *threads[0];

		for (i = 0; i < countof(threads); i++)
			threads[i] = thrCreate(0, &proc_idle, KernelThread);

		for (i = 0; i < countof(threads); i++)
			threads[i]->suspend--;
	}

	/* Start the shell */
	shell = procLoad(3, L"short.exe", NULL);
	if (!shell)
		_cputws(L"Failed to load shell\n");
	
	thr_idle.id = 0;
	while (!hndIsSignalled(shell))
	{
		/* Keep trying to schedule another task */
		//sysYield();
		//asm("int3");
	}

	wprintf(L"Shell has exited\n");
	keShutdown(SHUTDOWN_POWEROFF);
	return 0;
}