/* $Id: proc.c,v 1.5 2002/02/20 01:35:54 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/thread.h>
#include <kernel/fs.h>

#define DEBUG
#include <kernel/debug.h>

#include <stdlib.h>
#include <wchar.h>
#include <os/defs.h>

extern char kernel_stack_end[], scode[];
extern addr_t kernel_pagedir[];

process_t *proc_first, *proc_last;
unsigned proc_last_id;

process_info_t proc_idle_info =
{
	SYS_BOOT,					/* cwd */
	0,							/* id */
	(addr_t) scode,				/* base */
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
	kernel_pagedir,				/* page_dir */
	NULL,						/* handles */
	0,							/* handle_count */
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

process_t *ProcCreateProcess(const wchar_t *exe)
{
	process_t *proc;
	
	proc = malloc(sizeof(process_t));
	if (proc == NULL)
		return NULL;

	/*wprintf(L"ProcCreateProcess(%s)\n", exe);*/
	memset(proc, 0, sizeof(*proc));
	proc->page_dir_phys = MemAllocPageDir();
	proc->hdr.tag = 'proc';
	proc->hdr.file = __FILE__;
	proc->hdr.line = __LINE__;
	proc->handle_count = 2;
	proc->handles = malloc(proc->handle_count * sizeof(void*));
	proc->handles[0] = NULL;
	proc->handles[1] = proc;
	proc->vmm_end = PAGE_SIZE;
	proc->stack_end = 0x80000000;
	proc->exe = _wcsdup(exe);
	proc->info = NULL;
	proc->id = ++proc_last_id;

	LIST_ADD(proc, proc);
	return proc;
}

void ProcDeleteProcess(process_t *proc)
{
	thread_t *thr, *next;

	SemAcquire(&proc->sem_lock);
	for (thr = thr_first; thr; thr = next)
	{
		next = thr->all_next;
		if (thr->process == proc)
			ThrDeleteThread(thr);
	}

	free((wchar_t*) proc->exe);
	SemRelease(&proc->sem_lock);
	free(proc);
}

bool ProcFirstTimeInit(process_t *proc)
{
	context_t *ctx;
	process_info_t *info;
	wchar_t *ch;
	module_t *mod;
	thread_t *thr;

	/*SemAcquire(&proc->sem_lock);*/
	TRACE1("Creating process from %s\n", proc->exe);

	proc->info = info = VmmAlloc(1, NULL, 
		3 | MEM_READ | MEM_WRITE | MEM_ZERO | MEM_COMMIT);
	if (info == NULL)
	{
		SemRelease(&proc->sem_lock);
		return false;
	}

	ch = wcsrchr(proc->exe, '/');
	if (ch == NULL)
		wcscpy(info->cwd, L"/");
	else
		wcsncpy(info->cwd, proc->exe, ch - proc->exe);
	
	info->id = proc->id;

	info->std_in = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
	info->std_out = FsOpen(SYS_DEVICES L"/tty0", FILE_WRITE);
	
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
	TRACE1("Successful; continuing at %lx\n", mod->entry);
	ctx = ThrGetContext(current);
	ctx->eip = mod->entry;
	/*SemRelease(&proc->sem_lock);*/
	return true;
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
	proc_idle.handles = malloc(proc_idle.handle_count * sizeof(void*));
	proc_idle.handles[0] = NULL;
	proc_idle.handles[1] = &proc_idle;
	proc_idle.page_dir_phys = (addr_t) proc_idle.page_dir 
		- (addr_t) scode 
		+ kernel_startup.kernel_phys;
	return true;
}