#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <malloc.h>

extern process_t proc_idle;

//! Allocates an area of memory in the specified process's address space.
/*!
 *	The vmmArea() function can be used to get a vm_area_t structure from
 *		a pointer.
 *
 *	The memory can only be accessed from with in the specified process's 
 *		context.
 *
 *	\param	proc	The process which will access the memory
 *	\param	pages	The number of 4KB pages to allocate
 *	\param	start	The virtual address at which the block will start. If this 
 *		is NULL an address will be chosen (unless flags includes the 
 *		MEM_LITERAL flag).
 *
 *	\return	A pointer to the start of the block, or NULL if the block could
 *		not be allocated.
 */
void* vmmAlloc(process_t* proc, size_t pages, addr_t start, dword flags)
{
	vm_area_t *area, *collide;
	//addr_t oldEnd;
	
	area = malloc(sizeof(vm_area_t));
	if (!area)
		return NULL;

	if (start == NULL && (flags & MEM_LITERAL) == 0)
		start = proc->vmm_end;
	
	/*while ((collide = vmmArea(proc, (void*) start)))
	{
		wprintf(L"vmmAlloc: block at %08x collides with block at %08x\n", 
			start, (addr_t) collide->start);
		start = (addr_t) collide->start + collide->pages * PAGE_SIZE;
	}*/
	if ((collide = vmmArea(proc, (void*) start)))
	{
		wprintf(L"vmmAlloc: block at %08x collides with block at %08x\n", 
			start, (addr_t) collide->start);
		return NULL;
	}

	//wprintf(L"vmmAlloc: allocating %d page(s) at %x\n", pages, start);
	area->next = NULL;
	area->start = (void*) start;
	area->pages = pages;
	//area->phys = NULL;
	area->flags = flags;
	area->prev = proc->last_vm_area;
	area->is_committed = false;

	if (proc->last_vm_area)
		proc->last_vm_area->next = area;
	if (!proc->first_vm_area)
		proc->first_vm_area = area;

	proc->last_vm_area = area;

	//oldEnd = proc->vmm_end;
	while ((collide = vmmArea(proc, (void*) proc->vmm_end)))
		proc->vmm_end = (addr_t) collide->start + collide->pages * PAGE_SIZE;

	//if (oldEnd != proc->vmm_end)
		//wprintf(L"vmmAlloc: vmm_end adjusted from %08x to %08x\n", oldEnd, proc->vmm_end);

	if (flags & MEM_COMMIT)
		vmmCommit(proc, area);
	
	return (void*) start;
}

//! Frees an area of memory allocated by the vmmAlloc() function.
/*!
 *	\param	proc	The process which contains the area
 *	\param	area	The area of memory to be freed
 */
void vmmFree(process_t* proc, vm_area_t* area)
{
	//wprintf(L"vmmFree: %d @ %p => %x\n", area->pages, area->start, area->phys);

	vmmUncommit(proc, area);
	
	if (area->prev)
		area->prev->next = area->next;
	if (area->next)
		area->next->prev = area->prev;
	if (proc->first_vm_area == area)
		proc->first_vm_area = NULL;
	if (proc->last_vm_area == area)
		proc->last_vm_area = NULL;

	free(area);
}

//! Commits an area of memory, by mapping and initialising it.
/*!
 *	This function is called by vmmAlloc() if the MEM_COMMIT flag is specified.
 *	Otherwise, it will be called when a page fault occurs in the area.
 *
 *	Physical storage is allocated (unless the MEM_LITERAL flag is specified) and
 *		it is mapped into the address space of the specified process. If the 
 *		process is the one currently executing, that area of the address space
 *		is invalidated, allowing it to be accessed immediately. If the MEM_ZERO
 *		flag was specified then the area is zeroed; otherwise, the contents of
 *		the area are undefined.
 *
 *	\param	proc	The process which contains the area
 *	\param	area	The area to be committed
 *	\return	true if the area could be committed, false otherwise.
 */
bool vmmCommit(process_t* proc, vm_area_t* area)
{
	dword f;
	void* ptr;

	if (area->is_committed)
		vmmUncommit(proc, area);

	//wprintf(L"vmmCommit: committing %dKB: ", (area->pages * PAGE_SIZE) / 1024);

	f = PRIV_PRES;
	if ((area->flags & 3) == 3)
	{
		//wprintf(L"[3] ");
		f |= PRIV_USER;
	}
	else
	{
		//wprintf(L"[0] ");
		f |= PRIV_KERN;
	}

	if (area->flags & MEM_READ)
	{
		//wprintf(L"[rd] ");
		f |= PRIV_RD;
	}

	if (area->flags & MEM_WRITE)
	{
		//wprintf(L"[wr] ");
		f |= PRIV_WR;
	}

	if (area->flags & MEM_LITERAL)
		memMap(proc->page_dir, (addr_t) area->start, 
			(addr_t) area->start, area->pages, f);
	else
	{
		int page;
		addr_t virt, phys;

		virt = (addr_t) area->start;
		for (page = 0; page < area->pages; page++)
		{
			phys = memAlloc();
			if (!phys)
				return false;

			//wprintf(L"%x ", phys);
			memMap(proc->page_dir, virt, phys, 1, f);
			virt += PAGE_SIZE;
		}
	}
	
	//area->phys = phys;

	if (proc == current->process)
	{
		//wprintf(L"invalidating...");
		vmmInvalidate(area);
		ptr = (void*) area->start;

		if (area->flags & MEM_ZERO)
		{
			//wprintf(L"zeroing %p...", ptr);
			//memset(ptr, 0, PAGE_SIZE * area->pages);
			i386_lmemset32((addr_t) ptr, 0, PAGE_SIZE * area->pages);
		}
	}
	else
	{
		//wprintf(L"%x: not invalidating ", area->start);
		assert((area->flags & MEM_ZERO) == 0);
		//ptr = (void*) phys;
	}

	//wprintf(L"ok\n");
	return true;
}

//! Removes an area of memory from the address space of the specified process.
/*!
 *	The physical storage associated with the area is deallocated and the area is
 *		un-mapped from the address space of the process.
 *
 *	\param	proc	The process which contains the area
 *	\param	area	The area to be uncommitted
 */
void vmmUncommit(process_t* proc, vm_area_t* area)
{
	int i;
	byte* virt;
	addr_t phys;

	if (area->is_committed)
	{
		//memFree(area->phys, area->pages);
	
		virt = area->start;
		for (i = 0; i < area->pages; i++)
		{
			if ((area->flags & MEM_LITERAL) == 0)
			{
				phys = memTranslate(proc->page_dir, virt) & -PAGE_SIZE;
				assert(phys != NULL);
				memFree(phys);
			}

			memMap(proc->page_dir, (addr_t) virt, 0, 1, 0);
			virt += PAGE_SIZE;
		}

		area->is_committed = false;
	}
}

//! Updates the processor's page table cache associated with the specified area.
/*!
 *	It is necessary to invalidate pages after their linear-to-physical address
 *		mapping has changed if they are part of the current address space.
 *
 *	\param	area	The area to be invalidated
 */
void vmmInvalidate(vm_area_t* area)
{
	addr_t virt;

	for (virt = 0; virt < area->pages * PAGE_SIZE; virt += PAGE_SIZE)
		invalidate_page((byte*) area->start + virt);
}

//! Retrieves the vm_area_t structure associated with the specified address.
/*!
 *	\param	proc	The process which contains the area
 *	\param	ptr	The linear address which will be contained within the area 
 *		returned
 *	\return	A vm_area_t structure describing the area of memory around the 
 *		address, or NULL if the address had not been allocated.
 */
vm_area_t* vmmArea(process_t* proc, const void* ptr)
{
	vm_area_t* area;

	for (area = proc->first_vm_area; area; area = area->next)
		if ((addr_t) ptr >= (addr_t) area->start && 
			(addr_t) ptr < (addr_t) area->start + area->pages * PAGE_SIZE)
			return area;
	
	return NULL;
}