/* $Id: proc.c,v 1.9 2002/02/27 18:33:55 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/thread.h>
#include <kernel/fs.h>
#include <kernel/sched.h>

/*#define DEBUG*/
#include <kernel/debug.h>
#include <os/rtl.h>
#include <os/syscall.h>

#include <stdlib.h>
#include <wchar.h>
#include <os/defs.h>

extern char kernel_stack_end[], scode[];
extern addr_t kernel_pagedir[];

process_t *proc_first, *proc_last;
unsigned proc_last_id;

process_info_t proc_idle_info =
{
	0,							/* id */
	(addr_t) scode,				/* base */
	NULL, NULL,					/* std_in, std_out */
	SYS_BOOT,					/* cwd */
	L"",						/* cmdline */
};

module_t mod_kernel =
{
	NULL,						/* prev */
	NULL,						/* next */
	(unsigned) -1,				/* refs */
	SYS_BOOT L"/kernel.exe",	/* name */
	(addr_t) scode,				/* base */
	0x1000000,					/* length */
	NULL,						/* entry */
	NULL,						/* file */
	PAGE_SIZE,					/* sizeof_headers */
	true,						/* imported */
};

process_t proc_idle = 
{
	{
		0,						/* hdr.locks */
		'proc',					/* hdr.tag */
		NULL,					/* hdr.locked_by */
		0,						/* hdr.signal */
		{
			NULL,				/* hdr.waiting.first */
			NULL,				/* hdr.waiting.last */
			NULL,				/* hdr.current */
		},
		__FILE__,				/* hdr.file */
		__LINE__,				/* hdr.line */
	},
	NULL,						/* prev */
	NULL,						/* next */
	(addr_t) kernel_stack_end,	/* stack_end */
	0,							/* page_dir_phys */
	/*kernel_pagedir,*/				/* page_dir */
	NULL,						/* handles */
	0,							/* handle_count */
	0,							/* handle_allocated */
	&mod_kernel,				/* mod_first */
	&mod_kernel,				/* mod_last */
	NULL,						/* area_first */
	NULL,						/* area_last */
	0xe8000000,					/* vmm_end */
	{ 0 },						/* sem_vmm */
	{ 0 },						/* sem_lock */
	L"kernel",					/* exe */
	&proc_idle_info,			/* info */
};

handle_t ProcSpawnProcess(const wchar_t *exe, const process_info_t *defaults)
{
	process_t *proc;
	thread_t *thr;
	wchar_t temp[MAX_PATH];
	
	FsFullPath(exe, temp);
	current->process->hdr.copies++;
	proc = ProcCreateProcess(temp);
	if (proc == NULL)
		return NULL;

	proc->creator = current->process;
	proc->info = malloc(sizeof(*proc->info));
	if (defaults == NULL)
		defaults = proc->creator->info;
	memcpy(proc->info, defaults, sizeof(*defaults));

	thr = ThrCreateThread(proc, false, (void (*)(void*)) 0xdeadbeef, false, NULL, 16);
	ScNeedSchedule(true);
	return HndDuplicate(current->process, &proc->hdr);
}

process_t *ProcCreateProcess(const wchar_t *exe)
{
	process_t *proc;
	
	proc = malloc(sizeof(process_t));
	if (proc == NULL)
		return NULL;

	memset(proc, 0, sizeof(*proc));
	proc->page_dir_phys = MemAllocPageDir();
	proc->hdr.tag = 'proc';
	proc->hdr.file = __FILE__;
	proc->hdr.line = __LINE__;

	/* Copy number 1 is the process's handle to itself */
	proc->hdr.copies = 1;
	proc->handle_count = 2;
	proc->handle_allocated = 16;
	proc->handles = malloc(proc->handle_allocated * sizeof(void*));
	memset(proc->handles, 0, proc->handle_allocated * sizeof(void*));
	/*proc->handles[0] = NULL;*/
	proc->handles[1] = proc;
	proc->vmm_end = PAGE_SIZE;
	proc->stack_end = 0x80000000;
	proc->exe = _wcsdup(exe);
	proc->info = NULL;
	proc->id = ++proc_last_id;

	TRACE2("ProcCreateProcess(%s): page dir = %x\n", 
		exe, proc->page_dir_phys);
	LIST_ADD(proc, proc);
	return proc;
}

bool ProcFirstTimeInit(process_t *proc)
{
	context_t *ctx;
	process_info_t *info;
	wchar_t *ch;
	module_t *mod;
	thread_t *thr;
	uint32_t cr3;

	/*SemAcquire(&proc->sem_lock);*/
	__asm__("mov %%cr3,%0" : "=r" (cr3));
	TRACE3("Creating process from %s: page dir = %x = %x\n", 
		proc->exe, proc->page_dir_phys, cr3);
	
	info = VmmAlloc(1, NULL, 
		3 | MEM_READ | MEM_WRITE | MEM_ZERO | MEM_COMMIT);
	if (info == NULL)
	{
		SemRelease(&proc->sem_lock);
		return false;
	}

	if (proc->info != NULL)
	{
		handle_hdr_t *ptr;
		memcpy(info, proc->info, sizeof(*info));
		
		ptr = HndGetPtr(proc->creator, info->std_in, 0);
		if (ptr)
			info->std_in = HndDuplicate(proc, ptr);
		else
			info->std_in = NULL;

		ptr = HndGetPtr(proc->creator, info->std_out, 0);
		if (ptr)
			info->std_out = HndDuplicate(proc, ptr);
		else
			info->std_out = NULL;

		free(proc->info);
	}
	else
	{
		ch = wcsrchr(proc->exe, '/');

		if (ch == NULL)
			wcscpy(info->cwd, L"/");
		else
			wcsncpy(info->cwd, proc->exe, ch - proc->exe);
	}

	if (info->std_in == NULL)
		info->std_in = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
	if (info->std_out == NULL)
		info->std_out = FsOpen(SYS_DEVICES L"/tty0", FILE_WRITE);

	if (proc->creator)
		proc->creator->hdr.copies--;

	proc->info = info;
	info->id = proc->id;

	for (thr = thr_first; thr; thr = thr->all_next)
		if (thr->process == proc)
		{
			if (thr->info == NULL)
			{
				ThrAllocateThreadInfo(thr);
				assert(thr->info != NULL);
			}

			thr->info->process = info;
		}

	mod = PeLoad(proc, proc->exe, NULL);
	if (mod == NULL)
	{
		wprintf(L"ProcCreateProcess: failed to load %s\n", proc->exe);
		SemRelease(&proc->sem_lock);
		return false;
	}

	info->base = mod->base;
	/*ctx = ThrGetContext(current);*/
	ctx = ThrGetUserContext(current);
	ctx->eip = mod->entry;
	TRACE2("Successful; continuing at %lx, ctx = %p\n", ctx->eip, ctx);
	/*SemRelease(&proc->sem_lock);*/
	return true;
}

void ProcExitProcess(int code)
{
	thread_t *thr, *next;
	process_t *proc;
	unsigned i;

	proc = current->process;
	wprintf(L"Process %u exited with code %d: ", proc->id, code);
	
	SemAcquire(&proc->sem_lock);
	HndSignalPtr(&proc->hdr, true);

	for (thr = thr_first; thr; thr = next)
	{
		next = thr->all_next;
		if (thr->process == proc)
			ThrDeleteThread(thr);
	}

	for (i = 0; i < proc->handle_count; i++)
		if (proc->handles[i] != NULL &&
			proc->handles[i] != proc)
		{
			handle_hdr_t *hdr;
			hdr = proc->handles[i];
			TRACE3("ProcExitProcess: (notionally) closing handle %ld(%S, %d)\n", 
				i, hdr->file, hdr->line);
			/*HndClose(proc, i, 0);*/
		}

	free(proc->handles);
	proc->handles = NULL;
	proc->handle_count = 0;

	proc->hdr.copies--;
	if (proc->hdr.copies == 0)
	{
		free((wchar_t*) proc->exe);
		SemRelease(&proc->sem_lock);
		free(proc);
		wprintf(L"all handles freed\n");
	}
	else
	{
		SemRelease(&proc->sem_lock);
		wprintf(L"still has %u refs\n", proc->hdr.copies);
	}
}

bool ProcPageFault(process_t *proc, addr_t addr)
{
	vm_area_t *area;
	module_t *mod;
#ifdef DEBUG
	static unsigned nest;
	unsigned i;

	for (i = 0; i < nest; i++)
		_cputws(L"  ", 2);

	wprintf(L"(%u) ProcPageFault(%s, %lx)\n", nest++, proc->exe, addr);
#endif
	
	if (proc->mod_first == NULL &&
		addr == 0xdeadbeef)
	{
#ifdef DEBUG
		nest--;
		for (i = 0; i < nest; i++)
			_cputws(L"  ", 2);
		wprintf(L"(%u) First-time init\n", nest);
#endif
		return ProcFirstTimeInit(proc);
	}
	
	if (addr < 0x80000000 && addr >= proc->stack_end)
	{
		addr = PAGE_ALIGN(addr);
#ifdef DEBUG
		nest--;
		for (i = 0; i < nest; i++)
			_cputws(L"  ", 2);
		wprintf(L"(%u) Stack\n", nest);
#endif
		return MemMap(addr, MemAlloc(), addr + PAGE_SIZE, 
			PRIV_USER | PRIV_RD | PRIV_WR | PRIV_PRES);
	}

	FOREACH (area, proc->area)
		if (addr >= (addr_t) area->start &&
			addr < (addr_t) area->start + PAGE_SIZE * area->pages)
		{
#ifdef DEBUG
			nest--;
			for (i = 0; i < nest; i++)
				_cputws(L"  ", 2);
			wprintf(L"(%u) VMM area\n", nest);
#endif
			return VmmCommit(area, NULL, -1);
		}

	FOREACH (mod, proc->mod)
		if (PePageFault(proc, mod, addr))
		{
#ifdef DEBUG
			nest--;
			for (i = 0; i < nest; i++)
				_cputws(L"  ", 2);
			wprintf(L"(%u) PE section\n", nest);
#endif
			return true;
		}

#ifdef DEBUG
	nest--;
	for (i = 0; i < nest; i++)
		_cputws(L"  ", 2);
	wprintf(L"(%u) Not handled\n", nest);
#endif
	return false;
}

/*!
 *	\brief	Initializes the idle process
 *
 *	This function sets up the idle process's initial handle table and
 *	physical page directory pointer.
 *
 *	\return	\p true
 */
bool ProcInit(void)
{
	proc_idle.handle_count = 2;
	proc_idle.handle_allocated = 16;
	proc_idle.handles = malloc(proc_idle.handle_allocated * sizeof(void*));
	proc_idle.handles[0] = NULL;
	proc_idle.handles[1] = &proc_idle;
	proc_idle.page_dir_phys = (addr_t) kernel_pagedir/*proc_idle.page_dir */
		- (addr_t) scode 
		+ kernel_startup.kernel_phys;
	return true;
}