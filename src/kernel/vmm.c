#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <malloc.h>

extern process_t proc_idle;

void* vmmMap(process_t* proc, size_t pages, addr_t start, void *dest, 
			 unsigned type, dword flags)
{
	vm_area_t *area, *collide;
	//addr_t oldEnd;
	
	area = malloc(sizeof(vm_area_t));
	if (!area)
		return NULL;

	collide = NULL;
	if (start == NULL && (flags & MEM_LITERAL) == 0)
	{
		start = proc->vmm_end;
	
		collide = vmmArea(proc, (void*) start);
		assert(collide == NULL);

		while ((collide = vmmArea(proc, (void*) start)))
		{
			wprintf(L"vmmAlloc: new block at %08x(%x) collides with block at %08x(%x)\n", 
				start, pages * PAGE_SIZE, (addr_t) collide->start, collide->pages * PAGE_SIZE);
			start = (addr_t) collide->start + collide->pages * PAGE_SIZE;
		}
	}
	else if ((collide = vmmArea(proc, (void*) start)))
	{
		wprintf(L"vmmAlloc: block at %08x collides with block at %08x\n", 
			start, (addr_t) collide->start);
		return NULL;
	}

	if (collide)
		wprintf(L"vmmAlloc: moved block to %08x\n", start);

	//wprintf(L"vmmAlloc: allocating %d page(s) at %x\n", pages, start);
	area->next = NULL;
	area->start = (void*) start;
	area->owner = proc;
	area->pages = pages;
	//area->phys = NULL;
	area->flags = flags;
	area->prev = proc->last_vm_area;
	//area->is_committed = false;
	area->type = type;

	switch (type)
	{
	case VM_AREA_NORMAL:
		break;
	case VM_AREA_MAP:
		area->dest.phys_map = (addr_t) dest;
		break;
	case VM_AREA_SHARED:
		area->dest.shared_from = NULL;
		break;
	case VM_AREA_FILE:
		area->dest.file = (file_t*) dest;
		break;
	}

	semAcquire(&proc->sem_vmm);
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

	semRelease(&proc->sem_vmm);

	if (flags & MEM_COMMIT)
		vmmCommit(area, NULL, (size_t) -1);
	
	return (void*) start;
}

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
	return vmmMap(proc, pages, start, NULL, VM_AREA_NORMAL, flags);
}

void* vmmShare(vm_area_t* area, process_t* proc, addr_t start, dword flags)
{
	vm_area_t *new_area, *collide;

	if (start == NULL)
		start = (addr_t) area->start;

	if ((collide = vmmArea(proc, (void*) start)))
	{
		wprintf(L"vmmShare: block at %08x collides with block at %08x\n", 
			start, (addr_t) collide->start);
		return NULL;
	}

	new_area = malloc(sizeof(vm_area_t));
	new_area->next = NULL;
	new_area->start = (void*) start;
	new_area->owner = proc;
	new_area->pages = area->pages;
	//area->phys = NULL;
	new_area->flags = flags;
	new_area->prev = proc->last_vm_area;
	//new_area->is_committed = false;
	new_area->type = VM_AREA_SHARED;
	new_area->dest.shared_from = area;
	return new_area->start;
}

void* vmmMapFile(process_t *proc, addr_t start, size_t pages, file_t *file, 
				 dword flags)
{
	flags &= ~(MEM_COMMIT | MEM_ZERO);
	return vmmMap(proc, pages, start, file, VM_AREA_FILE, flags);
}

//! Frees an area of memory allocated by the vmmAlloc() function.
/*!
 *	\param	proc	The process which contains the area
 *	\param	area	The area of memory to be freed
 */
void vmmFree(vm_area_t* area)
{
	if (area == NULL)
		return;

	//wprintf(L"vmmFree: %d => %x...", area->pages, area->start);

	if (area->start != NULL)
		vmmUncommit(area);
	
	semAcquire(&area->owner->sem_vmm);
	if (area->prev)
		area->prev->next = area->next;
	if (area->next)
		area->next->prev = area->prev;
	if (area->owner->first_vm_area == area)
		area->owner->first_vm_area = NULL;
	if (area->owner->last_vm_area == area)
		area->owner->last_vm_area = NULL;
	semRelease(&area->owner->sem_vmm);

	free(area);
	//wprintf(L"done\n");
}

bool vmmDoMapFile(vm_area_t *area, addr_t start, size_t pages)
{
	IMAGE_DOS_HEADER *dos;
	IMAGE_PE_HEADERS *pe;

	wprintf(L"vmmDoMapFile: area = %x(%d) mapping file at %x(%d)\n",
		area->start, area->pages, start, pages);
	if (area->flags & MEM_FILE_IMAGE && start != 0)
	{
		halt(0);
		dos = (IMAGE_DOS_HEADER*) area->start;
		pe = (IMAGE_PE_HEADERS*) (area->start + dos->e_lfanew);
	}

	fsSeek(area->dest.file, start - (addr_t) area->start);
	fsRead(area->dest.file, (void*) start, pages * PAGE_SIZE);
	return true;
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
bool vmmCommit(vm_area_t* area, addr_t start, size_t pages)
{
	dword f;

	//if (area->is_committed)
		//vmmUncommit(area);

	if (start == NULL)
		start = (addr_t) area->start;
	if (pages == (size_t) -1)
		pages = area->pages;

	semAcquire(&area->owner->sem_vmm);
	//wprintf(L"vmmCommit: committing %dKB: ", (area->pages * PAGE_SIZE) / 1024);

	f = PRIV_PRES;
	if ((area->flags & 3) == 3)
		f |= PRIV_USER;
	else
		f |= PRIV_KERN;
	
	if (area->flags & MEM_READ)
		f |= PRIV_RD;
	
	if (area->flags & MEM_WRITE)
		f |= PRIV_WR;
	
	if (area->flags & MEM_LITERAL)
		memMap(area->owner->page_dir, (addr_t) area->start, 
			(addr_t) area->start, area->pages, f);
	else
	{
		int page;
		addr_t virt, phys;

		virt = start;
		if (area->type == VM_AREA_MAP)
			phys = area->dest.phys_map;
		else
			phys = NULL;
			
		for (page = 0; page < pages; page++)
		{
			if (area->type == VM_AREA_NORMAL ||
				area->type == VM_AREA_FILE)
			{
				phys = memAlloc();
				if (!phys)
				{
					semRelease(&area->owner->sem_vmm);
					return false;
				}
			}

			memMap(area->owner->page_dir, virt, phys, 1, f);
			virt += PAGE_SIZE;

			if (area->type == VM_AREA_MAP)
				phys += PAGE_SIZE;
		}
	}
	
	if (area->owner == current->process)
	{
		vmmInvalidate(area, start, pages);
		
		if (area->type == VM_AREA_FILE)
			vmmDoMapFile(area, start, pages);
		else if (area->flags & MEM_ZERO)
			i386_lmemset32(start, 0, PAGE_SIZE * pages);
	}
	else
	{
		assert((area->flags & MEM_ZERO) == 0);
		assert(area->type != VM_AREA_FILE);
	}

	//area->is_committed = true;
	semRelease(&area->owner->sem_vmm);
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
void vmmUncommit(vm_area_t* area)
{
	int i;
	byte* virt;
	addr_t phys;

	//wprintf(L"vmmUncommit: %d => %x...", area->pages, area->start);

	//if (area->is_committed)
	{
		//memFree(area->phys, area->pages);
	
		semAcquire(&area->owner->sem_vmm);
		virt = area->start;
		for (i = 0; i < area->pages; i++)
		{
			if ((area->flags & MEM_LITERAL) == 0)
			{
				phys = memTranslate(area->owner->page_dir, virt) & -PAGE_SIZE;
				if (phys)
					memFree(phys);
			}

			memMap(area->owner->page_dir, (addr_t) virt, 0, 1, 0);
			virt += PAGE_SIZE;
		}

		semRelease(&area->owner->sem_vmm);
		//area->is_committed = false;
	}

	//wprintf(L"done\n");
}

//! Updates the processor's page table cache associated with the specified area.
/*!
 *	It is necessary to invalidate pages after their linear-to-physical address
 *		mapping has changed if they are part of the current address space.
 *
 *	\param	area	The area to be invalidated
 */
void vmmInvalidate(vm_area_t* area, addr_t start, size_t pages)
{
	addr_t virt;

	if (start == NULL)
		start = (addr_t) area->start;
	if (pages == (size_t) -1)
		pages = area->pages;
	for (virt = 0; virt < pages * PAGE_SIZE; virt += PAGE_SIZE)
		invalidate_page((byte*) start + virt);
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