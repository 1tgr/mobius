#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/handle.h>
#include <kernel/thread.h>
#include <kernel/vmm.h>
#include <kernel/fs.h>

#include <malloc.h>
#include <wchar.h>
#include <errno.h>
#include <string.h>

extern dword kernel_pagedir[];
extern byte start[];
extern process_t proc_idle;

unsigned last_process_id;

bool procPageFault(process_t* proc, addr_t virt)
{
	addr_t phys, page;
	vm_area_t* area;

	if (virt >= proc->stack_end &&
		virt < 0x80000000)
	{
		phys = memAlloc();
		page = virt & -PAGE_SIZE;
		memMap(proc->page_dir, (addr_t) page, phys, 1, 
			PRIV_USER | PRIV_WR | PRIV_PRES);
		invalidate_page((void*) page);
		return true;
	}
	else
	{
		virt &= -PAGE_SIZE;
		for (area = proc->first_vm_area; area; area = area->next)
		{
			if (virt >= (addr_t) area->start &&
				virt < (addr_t) area->start + area->pages * PAGE_SIZE)
			{
				//if (!area->is_committed)
					return vmmCommit(area, virt, 1);
				/*else
				{
					wprintf(L"Area at %p already committed\n", area->start);
					errno = EBUFFER;
					return false;
				}*/
			}
		}
		
		errno = ENOTFOUND;
		return false;
	}
}

size_t mslen(const wchar_t* str)
{
	size_t size = 0, len;

	while (*str)
	{
		len = wcslen(str);
		str += len + 1;
		size += len;
	}

	return size;
}

const wchar_t* msncpy(wchar_t *dest, const wchar_t *src, size_t max)
{
	size_t len;
	wchar_t *ret = dest;

	while (*src)
	{
		wcsncpy(dest, src, max);

		len = wcslen(src);
		max -= len + 1;

		if (max <= 0)
			return ret;

		dest += len + 1;
		src += len + 1;
	}

	*dest = '\0';
	return ret;
}

//! Creates a new process, based on an image file on disk
/*!
 *	The process's fields are set up and the specified module is loaded, along
 *		with kernelu.dll. A new thread is created, at the same level as the
 *		process, at the entry point of the module. The new thread is not
 *		scheduled yet, but it is unsuspended.
 *
 *	\param	level	Level of the new process. Zero (kernel mode) and 3 (user
 *		mode) are supported.
 *	\param	file	Image file from which to load the process
 *	\param	cmdline	Command line to pass to the process
 *	\return	A new process_t structure representing the new process.
 */
process_t* procLoad(byte level, const wchar_t* file, const wchar_t* cmdline,
					unsigned priority, file_t* input, file_t* output)
{
	process_t* proc;
	thread_t* thr;
	module_t *mod, *kmod, *nmod;
	//dword kernel_start = (dword) start >> 20;
	process_info_t* info;
	vm_area_t* area;

	proc = hndAlloc(sizeof(process_t), NULL);
	if (!proc)
		return NULL;

	memset(proc, 0, sizeof(process_t));
	proc->page_dir = (dword*) memAlloc();
	assert(proc->page_dir != NULL);

	proc->stack_end = 0x80000000;
	proc->vmm_end = 0;
	memcpy(proc->page_dir, kernel_pagedir, PAGE_SIZE);
	proc->level = level;
	proc->last_vm_area = proc->first_vm_area = NULL;
	proc->marshal_map = NULL;
	proc->last_marshal = 0;
	proc->id = ++last_process_id;
	semInit(&proc->sem_vmm);

	proc->mod_first = proc->mod_last = NULL;
	mod = peLoad(proc, file, 0);
	
	if (mod == NULL)
	{
		procDelete(proc);
		return NULL;
	}

	peLoad(proc, L"kernelu.dll", 0);
	
	for (kmod = proc_idle.mod_first; kmod; kmod = kmod->next)
	{
		nmod = hndAlloc(sizeof(module_t), proc);

		*nmod = *kmod;
		nmod->name = wcsdup(kmod->name);

		if (proc->mod_last)
			proc->mod_last->next = nmod;
		if (proc->mod_first == NULL)
			proc->mod_first = nmod;
		nmod->prev = proc->mod_last;
		nmod->next = NULL;
		proc->mod_last = nmod;
	}
	
	vmmAlloc(proc, _sysinfo.memory_top / PAGE_SIZE, 0, 0);

	/* Allocate process memory for the user-mode process structure */
	proc->info = vmmAlloc(proc, 1, NULL, MEM_USER | MEM_COMMIT | MEM_READ | MEM_WRITE);
	assert(proc->info != NULL);
	//wprintf(L"%s: PCB at %x\n", proc->mod_first->name, proc->info);

	area = vmmArea(proc, proc->info);
	assert(area != NULL);

	if (area)
	{
		int pages;
		addr_t phys;
		process_info_t *cur = current->process->info;

		phys = memTranslate(proc->page_dir, area->start) & -PAGE_SIZE;

		/* Initialize the fields */
		info = (process_info_t*) phys;
		info->id = proc->id;
		info->root = cur->root;
		wcscpy(info->cwd, cur->cwd);
		info->base = mod->base;

#if 0
		if (cur->environment)
		{
			pages = (mslen(cur->environment) / sizeof(wchar_t) + PAGE_SIZE) / PAGE_SIZE;
			if (pages < 1)
				pages = 1;
		}
		else
			pages = 1;

		info->environment = vmmAlloc(proc, pages, NULL, 
			MEM_USER | MEM_COMMIT | MEM_READ | MEM_WRITE);
		if (cur->environment && info->environment)
		{
			vm_area_t *area;

			area = vmmArea(proc, info->environment);
			assert(area != NULL);

			if (area)
			{
				phys = memTranslate(current->process->page_dir, area->start) & -PAGE_SIZE;
				wprintf(L"procLoad: copying environment from %p to %x\n",
					cur->environment, phys);
				msncpy((wchar_t*) phys, cur->environment, pages * PAGE_SIZE);
			}
		}
#endif
		
		if (input == NULL)
			input = (file_t*) cur->input;
		if (output == NULL)
			output = (file_t*) cur->output;

		info->input = (addr_t) input;
		info->output = (addr_t) output;

		if (cmdline == NULL)
			cmdline = L"";

		/* Allocate process memory for the command line */
		if (wcslen(cmdline) + 1 <= PAGE_SIZE + sizeof(process_info_t))
		{
			//_cputws(L"Putting command line alongside process info...");
			info->cmdline = (wchar_t*) (proc->info + 1);
			wcscpy((wchar_t*) (info + 1), cmdline);
			//_cputws(L"done\n");
		}
		else
		{
			_cputws(L"Giving command line its own block\n");
			pages = (wcslen(cmdline) + PAGE_SIZE) & -PAGE_SIZE;
			info->cmdline = vmmAlloc(proc, pages, NULL, MEM_USER | MEM_COMMIT | 
				MEM_READ | MEM_WRITE | MEM_ZERO);

			area = vmmArea(proc, proc->info->cmdline);
			assert(area != NULL);
			if (area)
			{
				phys = memTranslate(current->process->page_dir, area->start) & -PAGE_SIZE;
				wcscpy((wchar_t*) phys, cmdline);
			}
		}
	}

	thr = thrCreate(level, proc, (void*) mod->entry, priority);
	thrSuspend(thr, false);
	return proc;
}

//! Closes a process handle. If the handle is freed then the process is terminated.
/*!
 *	Since a process object is a handle, freeing a process results in 
 *		decrementing its reference count (see hndFree()).
 *
 *	This function signals the process handle, terminates its threads and calls 
 *		hndFree(). If the handle was freed (and its reference count became
 *		zero) then all resources associated with the process are freed also.
 *
 *	\param	proc	Process to be freed
 */
void procDelete(process_t* proc)
{
	thread_t *thr, *next;
	vm_area_t *area, *anext;
	module_t *mod, *mnext;

	if (proc)
	{
		hndSignal(proc, true);
		
		/*if (proc->info)
		{
			if ((addr_t) proc->info->cmdline < (addr_t) proc->info + PAGE_SIZE)
				vmmFree(vmmArea(proc, proc->info->cmdline));

			if (proc->info->environment)
				vmmFree(vmmArea(proc, proc->info->environment));
		}*/

		for (thr = thr_first; thr; thr = next)
		{
			next = thr->next;

			if (thr->process == proc)
				thrDelete(thr);
		}
	
		if (hndHandle(proc)->refs == 0)
		{
			mod = proc->mod_first;
			while (mod)
			{
				mnext = mod->next;
				peUnload(proc, mod);
				mod = mnext;
			}
			
			for (area = proc->first_vm_area; area; area = anext)
			{
				anext = area->next;
				vmmFree(area);
			}

			memFree((addr_t) proc->page_dir);
		}

		hndEnum(proc);
		hndFree(proc);
	}
}

//! Terminates a process.
/*!
 *	Whereas procDelete() may only decrement a process's reference count,
 *		procTerminate() will always cause a process to stop executing and
 *		release all its resources.
 *
 *	\param	proc	Process to be terminated
 */
void procTerminate(process_t* proc)
{
	procDelete(proc);
}

process_t* procCurrent()
{
	return current->process;
}