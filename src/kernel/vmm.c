/* $Id: vmm.c,v 1.3 2002/01/05 00:54:11 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/fs.h>

#include <stdlib.h>
#include <stdio.h>

extern process_t proc_idle;

void* VmmMap(size_t pages, addr_t start, void *dest, unsigned type, 
			 uint32_t flags)
{
	vm_area_t *area, *collide;
	process_t *proc = current->process;
	
	area = malloc(sizeof(vm_area_t));
	if (!area)
		return NULL;

	collide = NULL;
	if (start == NULL && (flags & MEM_LITERAL) == 0)
	{
		start = proc->vmm_end;
	
		collide = VmmArea(proc, (void*) start);
		assert(collide == NULL);

		while ((collide = VmmArea(proc, (void*) start)))
		{
			wprintf(L"vmmAlloc: new block at %08x(%x) collides with block at %08x(%x)\n", 
				start, pages * PAGE_SIZE, (addr_t) collide->start, collide->pages * PAGE_SIZE);
			start = (addr_t) collide->start + collide->pages * PAGE_SIZE;
		}
	}
	else if ((collide = VmmArea(proc, (void*) start)))
	{
		wprintf(L"vmmAlloc: block at %08x collides with block at %08x\n", 
			start, (addr_t) collide->start);
		return NULL;
	}

	if (collide)
		wprintf(L"VmmMap: moved block to %08x\n", start);

	/*wprintf(L"VmmMap: allocating %d page(s) at %x\n", pages, start);*/
	area->start = (void*) start;
	area->owner = proc;
	area->pages = pages;
	area->flags = flags;
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
		area->dest.file = (handle_t) dest;
		break;
	}

	SemAcquire(&proc->sem_vmm);
	/*if (proc->last_vm_area)
		proc->last_vm_area->next = area;
	if (!proc->first_vm_area)
		proc->first_vm_area = area;

	proc->last_vm_area = area;*/
	LIST_ADD(proc->area, area);

	/*oldEnd = proc->vmm_end;*/
	while ((collide = VmmArea(proc, (void*) proc->vmm_end)))
		proc->vmm_end = (addr_t) collide->start + collide->pages * PAGE_SIZE;

	/*if (oldEnd != proc->vmm_end)
		wprintf(L"vmmAlloc: Vmm_end adjusted from %08x to %08x\n", oldEnd, proc->vmm_end);*/

	SemRelease(&proc->sem_vmm);

	if (flags & MEM_COMMIT)
		VmmCommit(area, NULL, (size_t) -1);
	
	return (void*) start;
}

/*!	\brief Allocates an area of memory in the specified process's address space.
 *	The VmmArea() function can be used to get a vm_area_t structure from
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
void* VmmAlloc(size_t pages, addr_t start, uint32_t flags)
{
	return VmmMap(pages, start, NULL, VM_AREA_NORMAL, flags);
}

/*
void* VmmShare(vm_area_t* area, process_t* proc, addr_t start, uint32_t flags)
{
	vm_area_t *new_area, *collide;

	if (start == NULL)
		start = (addr_t) area->start;

	if ((collide = VmmArea(proc, (void*) start)))
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
*/

void* VmmMapFile(addr_t start, size_t pages, handle_t file, uint32_t flags)
{
	flags &= ~(MEM_COMMIT | MEM_ZERO);
	return VmmMap(pages, start, (void*) file, VM_AREA_FILE, flags);
}

/*!	\brief Frees an area of memory allocated by the VmmAlloc() function.
 *	\param	proc	The process which contains the area
 *	\param	area	The area of memory to be freed
 */
void VmmFree(vm_area_t* area)
{
	if (area == NULL)
		return;

	/*wprintf(L"vmmFree: %d => %x...", area->pages, area->start);*/

	if (area->start != NULL)
		VmmUncommit(area);
	
	SemAcquire(&area->owner->sem_vmm);
	if (area->prev)
		area->prev->next = area->next;
	if (area->next)
		area->next->prev = area->prev;
	if (area->owner->area_first == area)
		area->owner->area_first = NULL;
	if (area->owner->area_last == area)
		area->owner->area_last = NULL;
	SemRelease(&area->owner->sem_vmm);

	free(area);
	/*wprintf(L"done\n");*/
}

bool VmmDoMapFile(vm_area_t *area, addr_t start, size_t pages)
{
	/* IMAGE_DOS_HEADER *dos;
	IMAGE_PE_HEADERS *pe;*/

	wprintf(L"vmmDoMapFile: area = %x(%d) mapping file at %x(%d)\n",
		area->start, area->pages, start, pages);
	/*if (area->flags & MEM_FILE_IMAGE && start != 0)
	{
		halt(0);
		dos = (IMAGE_DOS_HEADER*) area->start;
		pe = (IMAGE_PE_HEADERS*) (area->start + dos->e_lfanew);
	}*/

	FsSeek(area->dest.file, start - (addr_t) area->start);
	FsRead(area->dest.file, (void*) start, pages * PAGE_SIZE);
	return true;
}

/*!	\brief Commits an area of memory, by mapping and initialising it.
 *	This function is called by VmmAlloc() if the MEM_COMMIT flag is specified.
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
bool VmmCommit(vm_area_t* area, addr_t start, size_t pages)
{
	uint32_t f;

	assert(area->owner == current->process);
	if (start == NULL)
		start = (addr_t) area->start;
	if (pages == (size_t) -1)
		pages = area->pages;

	if ((f = MemTranslate((void*) start)))
	{
		wprintf(L"VmmCommit(%lx, %u): already mapped: %lx\n", 
			start, pages, f);
		return false;
	}

	SemAcquire(&area->owner->sem_vmm);
	/*wprintf(L"vmmCommit: committing %dKB: ", (area->pages * PAGE_SIZE) / 1024);*/

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
		MemMap((addr_t) area->start, (addr_t) area->start, 
			(addr_t) area->start + area->pages * PAGE_SIZE, f);
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
				phys = MemAlloc();
				if (!phys)
				{
					SemRelease(&area->owner->sem_vmm);
					return false;
				}
			}

			/*wprintf(L"%lx=>%lx|%lx ", virt, phys, f);*/
			MemMap(virt, phys, virt + PAGE_SIZE, f);
			virt += PAGE_SIZE;

			if (area->type == VM_AREA_MAP)
				phys += PAGE_SIZE;
		}
	}
	
	/*wprintf(L"\n");*/
	if (area->owner == current->process)
	{
		/*VmmInvalidate(area, start, pages);*/
		
		if (area->type == VM_AREA_FILE)
			VmmDoMapFile(area, start, pages);
		else if (area->flags & MEM_ZERO)
			memset((void*) start, 0, PAGE_SIZE * pages);
	}
	else
	{
		assert((area->flags & MEM_ZERO) == 0);
		assert(area->type != VM_AREA_FILE);
	}

	SemRelease(&area->owner->sem_vmm);
	return true;
}

/*!	\brief Removes an area of memory from the address space of the specified process.
 *	The physical storage associated with the area is deallocated and the area is
 *		un-mapped from the address space of the process.
 *
 *	\param	proc	The process which contains the area
 *	\param	area	The area to be uncommitted
 */
void VmmUncommit(vm_area_t* area)
{
	int i;
	uint8_t* virt;
	addr_t phys;

	assert(area->owner == current->process);
	/*wprintf(L"vmmUncommit: %d => %x...", area->pages, area->start);*/

	SemAcquire(&area->owner->sem_vmm);
	virt = area->start;
	for (i = 0; i < area->pages; i++)
	{
		if ((area->flags & MEM_LITERAL) == 0)
		{
			phys = MemTranslate(virt) & -PAGE_SIZE;
			if (phys)
				MemFree(phys);
		}

		MemMap((addr_t) virt, 0, (addr_t) virt + PAGE_SIZE, 0);
		virt += PAGE_SIZE;
	}

	SemRelease(&area->owner->sem_vmm);
	
	/*wprintf(L"done\n"); */
}

/*!	\brief Updates the processor's page table cache associated with the specified area.
 *	It is necessary to invalidate pages after their linear-to-physical address
 *		mapping has changed if they are part of the current address space.
 *
 *	\param	area	The area to be invalidated
 */
void VmmInvalidate(vm_area_t* area, addr_t start, size_t pages)
{
	addr_t virt;

	if (start == NULL)
		start = (addr_t) area->start;
	if (pages == (size_t) -1)
		pages = area->pages;
	for (virt = 0; virt < pages * PAGE_SIZE; virt += PAGE_SIZE)
		invalidate_page((uint8_t*) start + virt);
}

/*!	\brief Retrieves the vm_area_t structure associated with the specified address.
 *	\param	proc	The process which contains the area
 *	\param	ptr	The linear address which will be contained within the area 
 *		returned
 *	\return	A vm_area_t structure describing the area of memory around the 
 *		address, or NULL if the address had not been allocated.
 */
vm_area_t* VmmArea(process_t* proc, const void* ptr)
{
	vm_area_t* area;

	FOREACH (area, proc->area)
		if ((addr_t) ptr >= (addr_t) area->start && 
			(addr_t) ptr < (addr_t) area->start + area->pages * PAGE_SIZE)
			return area;
	
	return NULL;
}