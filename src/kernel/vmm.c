/* $Id: vmm.c,v 1.9 2002/04/20 12:30:04 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/fs.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

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
		start, pages * PAGE_SIZE, 
                collide->start, collide->pages->num_pages * PAGE_SIZE);
	    start = collide->start + collide->pages->num_pages * PAGE_SIZE;
	}
    }
    else if ((collide = VmmArea(proc, (void*) start)))
    {
	wprintf(L"vmmAlloc: block at %08x collides with block at %08x\n", 
	    start, collide->start);
	return NULL;
    }

    if (collide)
	wprintf(L"VmmMap: moved block to %08x\n", start);

    /*wprintf(L"VmmMap: allocating %d page(s) at %x\n", pages, start);*/
    area->start = start;
    area->owner = proc;
    area->pages = MemCopyPageArray(pages, 0, NULL);

    if (area->pages == NULL)
    {
        free(area);
        return false;
    }

    area->flags = flags;
    area->type = type;
    SemInit(&area->mtx_allocate);

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
    case VM_AREA_IMAGE:
        area->dest.mod = dest;
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
	proc->vmm_end = collide->start + collide->pages->num_pages * PAGE_SIZE;

    /*if (oldEnd != proc->vmm_end)
	wprintf(L"vmmAlloc: Vmm_end adjusted from %08x to %08x\n", oldEnd, proc->vmm_end);*/

    SemRelease(&proc->sem_vmm);

    /*if (flags & MEM_COMMIT)
	VmmCommit(area, NULL, (size_t) -1, (flags & MEM_WRITE) == MEM_WRITE);*/

    return (void*) start;
}

/*!    \brief Allocates an area of memory in the specified process's address space.
 *
 *    The VmmArea() function can be used to get a vm_area_t structure from
 *	  a pointer.
 *
 *    The memory can only be accessed from with in the specified process's 
 *	  context.
 *
 *    \param	proc	The process which will access the memory
 *    \param	pages	 The number of 4KB pages to allocate
 *    \param	start	 The virtual address at which the block will start. If this 
 *	  is NULL an address will be chosen (unless flags includes the 
 *	  MEM_LITERAL flag).
 *
 *    \return	 A pointer to the start of the block, or NULL if the block could
 *	  not be allocated.
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

/*!    \brief Frees an area of memory allocated by the VmmAlloc() function.
 *
 *    \param	proc	The process which contains the area
 *    \param	area	The area of memory to be freed
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

bool VmmMapAddressToFile(vm_area_t *area, addr_t addr, uint64_t *off, 
                         size_t *bytes, uint32_t *flags)
{
    if (area->type == VM_AREA_IMAGE)
    {
        assert(area->start == area->dest.mod->base);
        return PeMapAddressToFile(area->dest.mod, addr, off, bytes, flags);
    }
    else
    {
        *off = addr - area->start;
        *bytes = PAGE_SIZE;
        *flags = area->flags;
        return true;
    }
}

bool VmmDoMapFile(vm_area_t *area, addr_t start, bool is_writing)
{
    uint32_t flags;
    uint16_t state;
    addr_t phys, voff;
    uint64_t off;
    size_t bytes;
    handle_t file;

    assert(area->owner == current->process);

    voff = start - area->start;
    state = MemGetPageState((const void*) start);
    SemAcquire(&area->owner->sem_vmm);
    //wprintf(L"VmmDoMapFile: start = %x ", start);

tryagain:
    switch (state)
    {
    case PAGE_PAGEDOUT:
        wprintf(L"PAGE_PAGEDOUT\n");

    case 0: /* not committed */
        if (!VmmMapAddressToFile(area, start, &off, &bytes, &flags))
        {
            MemSetPageState((const void*) start, PAGE_READFAILED);
            SemRelease(&area->owner->sem_vmm);
            return true;
        }

        //wprintf(L"off = %x bytes = %x flags = %x\n", (uint32_t) off, bytes, flags);

        MtxAcquire(&area->mtx_allocate);
        phys = MemAlloc();
        MemLockPages(phys, 1, true);
        area->pages->pages[voff / PAGE_SIZE] = phys;

        area->flags = (area->flags & ~(MEM_READ | MEM_WRITE | MEM_ZERO)) 
            | (flags & (MEM_READ | MEM_WRITE | MEM_ZERO));
        
        if (off == (uint64_t) -1 ||
            bytes == (size_t) -1)
        {
            //wprintf(L"VmmDoMapFile: empty space, zeroing\n");

            if (!MemMap(start, phys, start + PAGE_SIZE, PAGE_VALID_DIRTY))
                MemSetPageState((const void*) start, PAGE_READFAILED);
            else
            {
                memset((void*) start, 0, PAGE_SIZE);

                if (is_writing && (area->flags & MEM_WRITE))
                    state = PAGE_VALID_DIRTY;
                else
                    state = PAGE_VALID_CLEAN;

                if ((area->flags & 3) == 3)
	            state |= PRIV_USER;
                else
	            state |= PRIV_KERN;

                MemSetPageState((const void*) start, state);
            }

            MtxRelease(&area->mtx_allocate);
            SemRelease(&area->owner->sem_vmm);
            return true;
        }
        else
        {
            if (area->type == VM_AREA_IMAGE)
                file = area->dest.mod->file;
            else
                file = area->dest.file;

            area->pagingop.event = file;
            MemSetPageState((const void*) start, PAGE_READINPROG);

            area->read_pages = MemCopyPageArray(1, 0, &phys);
            FsSeek(file, off, FILE_SEEK_SET);
            if (!FsReadPhysical(file, area->read_pages, bytes, &area->pagingop))
                MemSetPageState((const void*) start, PAGE_READFAILED);

            MtxRelease(&area->mtx_allocate);
            state = PAGE_READINPROG;
            goto tryagain;
        }

    case PAGE_VALID_CLEAN:
        wprintf(L"%s:%x: PAGE_VALID_CLEAN ", area->owner->exe, start);
        if (area->flags & MEM_WRITE)
        {
            wprintf(L"=> PAGE_VALID_DIRTY\n");
            MemSetPageState((const void*) start, PAGE_VALID_DIRTY);
            SemRelease(&area->owner->sem_vmm);
            return true;
        }

        break;

    case PAGE_VALID_DIRTY:
        wprintf(L"PAGE_VALID_DIRTY\n");
        break;

    case PAGE_READINPROG:
        //wprintf(L"PAGE_READINPROG\n");
        MtxAcquire(&area->mtx_allocate);
        if (EvtIsSignalled(NULL, area->pagingop.event))
        {
            MemDeletePageArray(area->read_pages);
            area->read_pages = NULL;

            if (area->pagingop.result == 0)
            {
                //wprintf(L"Read succeeded\n");
                if (is_writing && (area->flags & MEM_WRITE))
                    state = PAGE_VALID_DIRTY;
                else
                    state = PAGE_VALID_CLEAN;

                if ((area->flags & 3) == 3)
	            state |= PRIV_USER;
                else
	            state |= PRIV_KERN;

                if (!MemMap(start, area->pages->pages[voff / PAGE_SIZE], 
                    start + PAGE_SIZE, state))
                    MemSetPageState((const void*) start, PAGE_READFAILED);

                //wprintf(L"Finished\n");
            }
            else
            {
                wprintf(L"PAGE_READINPROG(%x): read failed: %d\n", 
                    start, area->pagingop.result);
                MemSetPageState((const void*) start, PAGE_READFAILED);
            }
        }
        else
            ThrWaitHandle(current, area->pagingop.event, 0);

        MtxRelease(&area->mtx_allocate);
        SemRelease(&area->owner->sem_vmm);

        if (area->type == VM_AREA_IMAGE &&
            MemGetPageState((const void*) start) != PAGE_READINPROG)
            PeInitImage(area->dest.mod);
        return true;

    case PAGE_WRITEINPROG:
        wprintf(L"PAGE_WRITEINPROG\n");
        MtxAcquire(&area->mtx_allocate);
        MtxRelease(&area->mtx_allocate);
        SemRelease(&area->owner->sem_vmm);
        return true;

    case PAGE_READFAILED:
        wprintf(L"PAGE_READFAILED\n");
        break;

    case PAGE_WRITEFAILED:
        wprintf(L"PAGE_WRITEFAILED\n");
        break;

    default:
        wprintf(L"Unknown state: %x\n", state);
    }

    SemRelease(&area->owner->sem_vmm);
    return false;
}

/*!    \brief Commits an area of memory, by mapping and initialising it.
 *
 *    This function is called by VmmAlloc() if the MEM_COMMIT flag is specified.
 *    Otherwise, it will be called when a page fault occurs in the area.
 *
 *    Physical storage is allocated (unless the MEM_LITERAL flag is specified) and
 *	  it is mapped into the address space of the specified process. If the 
 *	  process is the one currently executing, that area of the address space
 *	  is invalidated, allowing it to be accessed immediately. If the MEM_ZERO
 *	  flag was specified then the area is zeroed; otherwise, the contents of
 *	  the area are undefined.
 *
 *    \param	proc	The process which contains the area
 *    \param	area	The area to be committed
 *    \return	 true if the area could be committed, false otherwise.
 */
bool VmmCommit(vm_area_t* area, addr_t start, bool is_writing)
{
    uint32_t f;
    uint16_t state;
    addr_t phys;

    assert(area->owner == current->process);

    state = MemGetPageState((const void*) start);
    SemAcquire(&area->owner->sem_vmm);
    switch (state)
    {
    case 0: /* not committed */
        if (is_writing && (area->flags & MEM_WRITE))
            f = PAGE_VALID_DIRTY;
        else
            f = PAGE_VALID_CLEAN;
        
        if ((area->flags & 3) == 3)
	    f |= PRIV_USER;
        else
	    f |= PRIV_KERN;

        if (area->flags & MEM_LITERAL)
	    MemMap(start, start, start + PAGE_SIZE, f);
        else
        {
	    if (area->type == VM_AREA_MAP)
	        phys = area->dest.phys_map + area->start - start;
	    else
            {
	        phys = MemAlloc();
	        if (!phys)
	        {
                    wprintf(L"VmmCommit: out of memory\n");
		    SemRelease(&area->owner->sem_vmm);
		    return false;
	        }
            }

	    MemMap(start, phys, start + PAGE_SIZE, f);
        }

        if (area->flags & MEM_ZERO)
            memset((void*) start, 0, PAGE_SIZE);

        SemRelease(&area->owner->sem_vmm);
        return true;

    case PAGE_VALID_CLEAN:
        wprintf(L"%s:%x: PAGE_VALID_CLEAN ", area->owner->exe, start);
        if (area->flags & MEM_WRITE)
        {
            wprintf(L"=> PAGE_VALID_DIRTY\n");
            MemSetPageState((const void*) start, PAGE_VALID_DIRTY);
            SemRelease(&area->owner->sem_vmm);
	    return true;
        }

        break;

    case PAGE_VALID_DIRTY:
        wprintf(L"PAGE_VALID_DIRTY\n");
        break;

    case PAGE_READINPROG:
        wprintf(L"PAGE_READINPROG\n");
        break;

    case PAGE_WRITEINPROG:
        wprintf(L"PAGE_WRITEINPROG\n");
        break;

    case PAGE_READFAILED:
        wprintf(L"PAGE_READFAILED\n");
        break;

    case PAGE_WRITEFAILED:
        wprintf(L"PAGE_WRITEFAILED\n");
        break;

    case PAGE_PAGEDOUT:
        wprintf(L"PAGE_PAGEDOUT\n");
        break;

    default:
        wprintf(L"Unknown state: %x\n", state);
    }

    SemRelease(&area->owner->sem_vmm);
    return false;
}

bool VmmPageFault(vm_area_t *area, addr_t page, bool is_writing)
{
    page = PAGE_ALIGN(page);

    switch (area->type)
    {
    case VM_AREA_NORMAL:
    case VM_AREA_MAP:
        return VmmCommit(area, page, is_writing);

    case VM_AREA_FILE:
    case VM_AREA_IMAGE:
        return VmmDoMapFile(area, page, is_writing);
    }

    return false;
}

/*!    \brief Removes an area of memory from the address space of the specified process.
 *
 *    The physical storage associated with the area is deallocated and the area is
 *	  un-mapped from the address space of the process.
 *
 *    \param	proc	The process which contains the area
 *    \param	area	The area to be uncommitted
 */
void VmmUncommit(vm_area_t* area)
{
    int i;
    addr_t virt, phys;

    assert(area->owner == current->process);
    /*wprintf(L"vmmUncommit: %d => %x...", area->pages, area->start);*/

    SemAcquire(&area->owner->sem_vmm);
    virt = area->start;
    for (i = 0; i < area->pages->num_pages; i++)
    {
	if ((area->flags & MEM_LITERAL) == 0)
	{
	    phys = MemTranslate((const void*) virt) & -PAGE_SIZE;
	    if (phys)
		MemFree(phys);
	}

	MemMap(virt, 0, virt + PAGE_SIZE, 0);
	virt += PAGE_SIZE;
    }

    SemRelease(&area->owner->sem_vmm);
    
    /*wprintf(L"done\n"); */
}

/*!    \brief Updates the processor's page table cache associated with the specified area.
 *
 *    It is necessary to invalidate pages after their linear-to-physical address
 *	  mapping has changed if they are part of the current address space.
 *
 *    \param	area	The area to be invalidated
 */
/*void VmmInvalidate(vm_area_t* area, addr_t start, size_t pages)
{
    addr_t virt;

    if (start == NULL)
	start = area->start;
    if (pages == (size_t) -1)
	pages = area->pages->num_pages;
    for (virt = 0; virt < pages * PAGE_SIZE; virt += PAGE_SIZE)
	invalidate_page((uint8_t*) start + virt);
}*/

/*!    \brief Retrieves the vm_area_t structure associated with the specified address.
 *
 *    \param	proc	The process which contains the area
 *    \param	ptr    The linear address which will be contained within the area 
 *	  returned
 *    \return	 A vm_area_t structure describing the area of memory around the 
 *	  address, or NULL if the address had not been allocated.
 */
vm_area_t* VmmArea(process_t* proc, const void* ptr)
{
    vm_area_t* area;

    FOREACH (area, proc->area)
	if ((addr_t) ptr >= area->start && 
	    (addr_t) ptr < area->start + area->pages->num_pages * PAGE_SIZE)
	    return area;
    
    return NULL;
}
