/* $Id: vmm.c,v 1.11 2002/05/19 13:04:59 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/vmm.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/fs.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>

extern process_t proc_idle;

vm_area_t *shared_first, *shared_last;
semaphore_t sem_share;

void* VmmMap(size_t pages, addr_t start, void *dest, unsigned type, 
             uint32_t flags)
{
    vm_area_t *area, *collide;
    bool is_kernel;
    process_t *proc;

    is_kernel = ((flags & 3) == 0) || 
        (type == VM_AREA_IMAGE && start >= 0x80000000);
    proc = current->process;
    area = malloc(sizeof(vm_area_t));
    if (!area)
        return NULL;

    collide = NULL;
    start = PAGE_ALIGN(start);
    if (start == NULL && (flags & MEM_LITERAL) == 0)
    {
        for (collide = proc->area_first; collide != NULL; collide = collide->next)
            if (collide->type == VM_AREA_EMPTY &&
                collide->num_pages >= pages &&
                (( is_kernel && collide->start >= 0x80000000) ||
                 (!is_kernel && collide->start <  0x80000000)))
                break;

        if (collide == NULL)
        {
            wprintf(L"VmmMap(%u, %x): out of address space\n", 
                pages * PAGE_SIZE, start);
            free(area);
            errno = ENOMEM;
            __asm__("int3");
            return NULL;
        }

        start = collide->start;
        collide->start += pages * PAGE_SIZE;
        collide->num_pages -= pages;
    }
    else
    {
        /*wprintf(L"VmmMap(%u, %x): area_first = %p, size = %u, start = %x\n",
            pages * PAGE_SIZE,
            start,
            proc->area_first,
            proc->area_first->num_pages * PAGE_SIZE,
            proc->area_first->start);*/

        collide = VmmArea(proc, (void*) start);
        assert(collide != NULL);
        if (collide->type != VM_AREA_EMPTY)
        {
            wprintf(L"VmmMap(%u, %x): collided with page at %x\n", 
                pages * PAGE_SIZE, start, collide->start);
            free(area);
            errno = EACCESS;
            return NULL;
        }

        if (collide->start == start)
        {
            collide->start += pages * PAGE_SIZE;
            collide->num_pages -= pages;
        }
        else if (collide->start + collide->num_pages * PAGE_SIZE 
            == start + pages * PAGE_SIZE)
            collide->num_pages -= pages;
        else
        {
            vm_area_t *collide2;

            assert(collide->start + collide->num_pages * PAGE_SIZE >
                (start + pages * PAGE_SIZE));

            collide2 = malloc(sizeof(vm_area_t));
            memset(collide2, 0, sizeof(vm_area_t));
            collide2->start = start + pages * PAGE_SIZE;
            collide2->num_pages = 
                (collide->start + collide->num_pages * PAGE_SIZE -
                (start + pages * PAGE_SIZE)) / PAGE_SIZE;
            collide2->type = VM_AREA_EMPTY;
            collide->num_pages = (start - collide->start) / PAGE_SIZE;

            SemAcquire(&proc->sem_vmm);
            LIST_ADD(proc->area, collide2);
            SemRelease(&proc->sem_vmm);
        }
    }

    area->start = start;
    area->owner = proc;
    area->pages = MemCopyPageArray(pages, 0, NULL);
    area->name = NULL;
    area->shared_prev = area->shared_next = NULL;
    area->num_pages = pages;

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
        area->dest.shared_from = dest;
        break;
    case VM_AREA_FILE:
        area->dest.file = (handle_t) dest;
        break;
    case VM_AREA_IMAGE:
        area->dest.mod = dest;
        break;
    }

    SemAcquire(&proc->sem_vmm);
    LIST_ADD(proc->area, area);
    SemRelease(&proc->sem_vmm);

    return (void*) start;
}

/*!    \brief Allocates an area of memory in the specified process's address space.
*
*    The VmmArea() function can be used to get a vm_area_t structure from
*      a pointer.
*
*    The memory can only be accessed from with in the specified process's 
*      context.
*
*    \param    proc    The process which will access the memory
*    \param    pages     The number of 4KB pages to allocate
*    \param    start     The virtual address at which the block will start. If this 
*      is NULL an address will be chosen (unless flags includes the 
*      MEM_LITERAL flag).
*
*    \return     A pointer to the start of the block, or NULL if the block could
*      not be allocated.
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

void *VmmMapFile(handle_t file, addr_t start, size_t pages, uint32_t flags)
{
    flags &= ~(MEM_COMMIT | MEM_ZERO);
    return VmmMap(pages, start, (void*) file, VM_AREA_FILE, flags);
}

bool VmmShare(void *base, const wchar_t *name)
{
    vm_area_t *area;

    area = VmmArea(current->process, base);
    if (area == NULL)
    {
        errno = ENOTFOUND;
        return false;
    }

    SemAcquire(&current->process->sem_vmm);
    free(area->name);
    area->name = _wcsdup(name);
    
    SemAcquire(&sem_share);

    if (shared_last != NULL)
        shared_last->shared_next = area;
    area->shared_prev = shared_last;
    area->shared_next = NULL;
    shared_last = area;
    if (shared_first == NULL)
        shared_first = area;

    SemRelease(&sem_share);

    SemRelease(&current->process->sem_vmm);
    return true;
}

void *VmmMapShared(const wchar_t *name, addr_t start, uint32_t flags)
{
    vm_area_t *dest;

    SemAcquire(&sem_share);

    for (dest = shared_first; dest != NULL; dest = dest->shared_next)
    {
        assert(dest->name != NULL);
        if (_wcsicmp(dest->name, name) == 0)
            break;
    }

    if (dest == NULL)
    {
        errno = ENOTFOUND;
        SemRelease(&sem_share);
        return NULL;
    }

    SemRelease(&sem_share);
    return VmmMap(dest->pages->num_pages, start, dest, VM_AREA_SHARED, flags);
}

/*!    \brief Frees an area of memory allocated by the VmmAlloc() function.
*
*    \param    proc    The process which contains the area
*    \param    area    The area of memory to be freed
*/
void VmmFree(vm_area_t* area)
{
    if (area == NULL)
        return;

    /*wprintf(L"vmmFree: %d => %x...", area->pages, area->start);*/

    if (area->start != NULL)
        VmmUncommit(area);

    SemAcquire(&area->owner->sem_vmm);
    /*if (area->prev)
        area->prev->next = area->next;
    if (area->next)
        area->next->prev = area->prev;
    if (area->owner->area_first == area)
        area->owner->area_first = NULL;
    if (area->owner->area_last == area)
        area->owner->area_last = NULL;*/

    MemDeletePageArray(area->pages);
    area->pages = NULL;
    free(area->name);
    area->name = NULL;

    area->type = VM_AREA_EMPTY;

    SemRelease(&area->owner->sem_vmm);

    /*free(area);*/
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

    assert(area->owner == current->process || 
        area->owner == &proc_idle);

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

        /*wprintf(L"%s:%x: PAGE_PAGEDOUT off = %x bytes = %x flags = %x\n", 
            area->owner->exe, start, (uint32_t) off, bytes, flags);*/

        MtxAcquire(&area->mtx_allocate);
        phys = MemAlloc();
        MemLockPages(phys, 1, true);
        area->pages->pages[voff / PAGE_SIZE] = phys;

        area->flags = (area->flags & ~(3 | MEM_READ | MEM_WRITE | MEM_ZERO)) 
            | (flags & (3 | MEM_READ | MEM_WRITE | MEM_ZERO));

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
                /*wprintf(L"%s:%x: finished blank page\n", area->owner->exe, start);*/
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
        /*wprintf(L"%s:%x: PAGE_VALID_CLEAN ", area->owner->exe, start);*/
        if (area->flags & MEM_WRITE)
        {
            /*wprintf(L"=> PAGE_VALID_DIRTY\n");*/
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
        if (/*EvtIsSignalled(NULL, area->pagingop.event) &&*/
            area->pagingop.result != -1)
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
                /*wprintf(L"%s:%x: finished page in, %u bytes read\n", 
                    area->owner->exe, start, area->pagingop.bytes);
                if (start == 0x618000)
                {
                    char *ptr;
                    ptr = (char*) start + PAGE_SIZE - 4;
                    wprintf(L"contents: %p = %02x %p = %02x %p = %02x %p = %02x\n", 
                        ptr + 0, ptr[0], ptr + 1, ptr[1], ptr + 2, ptr[2], ptr + 3, ptr[3]);
                }*/
            }
            else
            {
                wprintf(L"PAGE_READINPROG(%x): read failed: %d\n", 
                    start, area->pagingop.result);
                MemSetPageState((const void*) start, PAGE_READFAILED);
            }
        }
        else
        {
            wprintf(L"PAGE_READINPROG(%x): waiting, result = %d\n", 
                start, area->pagingop.result);
            ThrWaitHandle(current, area->pagingop.event, 0);
        }

        MtxRelease(&area->mtx_allocate);
        SemRelease(&area->owner->sem_vmm);

        if (area->type == VM_AREA_IMAGE &&
            MemGetPageState((const void*) start) != PAGE_READINPROG &&
            !area->dest.mod->imported)
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
        wprintf(L"%08x: unknown state for mapped area: %x\n", start, state);
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
*      it is mapped into the address space of the specified process. If the 
*      process is the one currently executing, that area of the address space
*      is invalidated, allowing it to be accessed immediately. If the MEM_ZERO
*      flag was specified then the area is zeroed; otherwise, the contents of
*      the area are undefined.
*
*    \param    proc    The process which contains the area
*    \param    area    The area to be committed
*    \return     true if the area could be committed, false otherwise.
*/
bool VmmCommit(vm_area_t* area, addr_t start, bool is_writing)
{
    uint32_t f;
    uint16_t state;
    addr_t phys;
    unsigned page;

    assert(area->owner == current->process || area->owner == &proc_idle);

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

        page = PAGE_ALIGN_UP(start - area->start) / PAGE_SIZE;
        if (area->flags & MEM_LITERAL)
            MemMap(start, start, start + PAGE_SIZE, f);
        else
        {
            if (area->type == VM_AREA_MAP)
                phys = area->dest.phys_map + (start - area->start);
            else if (area->type == VM_AREA_SHARED)
            {
                assert(area->dest.shared_from != NULL);
                if (page >= area->dest.shared_from->pages->num_pages)
                {
                    wprintf(L"VmmCommit(%lx): shared page index (%u) greater than number of pages (%u)\n",
                        start, page, area->dest.shared_from->pages->num_pages);
                    return false;
                }

                if (area->dest.shared_from->type == VM_AREA_MAP)
                    phys = area->dest.shared_from->dest.phys_map + (start - area->start);
                else
                {
                    phys = area->dest.shared_from->pages->pages[page];

                    if (phys == NULL)
                    {
                        wprintf(L"VmmCommit(%lx): page is not already mapped in shared area\n", 
                            start);
                        return false;
                    }
                }
            }
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

        area->pages->pages[page] = phys;
        MemLockPages(phys, 1, true);
        if (area->flags & MEM_ZERO)
            memset((void*) start, 0, PAGE_SIZE);

        SemRelease(&area->owner->sem_vmm);
        return true;

    case PAGE_VALID_CLEAN:
        /*wprintf(L"%s:%x: PAGE_VALID_CLEAN ", area->owner->exe, start);*/
        if (area->flags & MEM_WRITE)
        {
            /*wprintf(L"=> PAGE_VALID_DIRTY\n");*/
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
        wprintf(L"%08x: unknown state for normal area: %x\n", start, state);
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
    case VM_AREA_SHARED:
        return VmmCommit(area, page, is_writing);

    case VM_AREA_FILE:
    case VM_AREA_IMAGE:
        return VmmDoMapFile(area, page, is_writing);

    case VM_AREA_EMPTY:
        wprintf(L"VmmPageFault(%x): %s of empty area at %x\n",
            page, is_writing ? L"write" : L"read", area->start);
        break;
    }

    return false;
}

/*!    \brief Removes an area of memory from the address space of the specified process.
*
*    The physical storage associated with the area is deallocated and the area is
*      un-mapped from the address space of the process.
*
*    \param    proc    The process which contains the area
*    \param    area    The area to be uncommitted
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
        phys = area->pages->pages[i];
        if (phys)
        {
            MemLockPages(phys, 1, false);
            if (!MemIsPageLocked(phys))
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
*      mapping has changed if they are part of the current address space.
*
*    \param    area    The area to be invalidated
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
*    \param    proc    The process which contains the area
*    \param    ptr    The linear address which will be contained within the area 
*      returned
*    \return     A vm_area_t structure describing the area of memory around the 
*      address, or NULL if the address had not been allocated.
*/
vm_area_t* VmmArea(process_t* proc, const void* ptr)
{
    vm_area_t* area;
    addr_t p;

    p = PAGE_ALIGN((addr_t) ptr) / PAGE_SIZE;
    FOREACH (area, proc->area)
        if (p >= area->start / PAGE_SIZE && 
            /*
             * xxx - byte-granular address arithmetic overflows with 32-bit 
             *  pointers
             */
            p < area->start / PAGE_SIZE + area->num_pages)
        return area;

    return NULL;
}
