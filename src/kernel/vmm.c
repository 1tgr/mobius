/* $Id: vmm.c,v 1.18 2002/09/08 20:25:09 pavlovskii Exp $ */

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

vm_desc_t *shared_first, *shared_last;
spinlock_t sem_share;

/*
 * Finds the vm_node_t structure for a given virtual address
 */
vm_node_t *VmmLookupNode(vm_node_t *parent, const void *ptr)
{
    addr_t base_pages;

    base_pages = (addr_t) ptr / PAGE_SIZE;
    if (parent == NULL)
        return NULL;
    else if ((addr_t) ptr < parent->base)
        return VmmLookupNode(parent->left, ptr);
    else if (VMM_NODE_IS_EMPTY(parent) && 
        base_pages < parent->base / PAGE_SIZE + parent->u.empty_pages)
        return parent;
    else if (!VMM_NODE_IS_EMPTY(parent) && 
        base_pages < parent->base / PAGE_SIZE + parent->u.desc->num_pages)
        return parent;
    else
        return VmmLookupNode(parent->right, ptr);
}

/*
 * Inserts a vm_node_t structure into a tree for VmmAllocNode
 */
static void VmmInsertNode(vm_node_t *parent, vm_node_t *child)
{
    if (child->base < parent->base)
    {
        if (parent->left == NULL)
            parent->left = child;
        else
            VmmInsertNode(parent->left, child);
    }
    else
    {
        if (parent->right == NULL)
            parent->right = child;
        else
            VmmInsertNode(parent->right, child);
    }
}

/*
 * Find an empty VMM node of sufficient size for VmmAllocNode to use when 
 *  choosing a base address. Allocates from low to high (checks left, middle
 *  then right).
 */
static vm_node_t *VmmFindEmptyNode(vm_node_t *parent, unsigned pages, bool is_kernel)
{
    vm_node_t *ret;

    if (parent == NULL)
        return NULL;

    if ((ret = VmmFindEmptyNode(parent->left, pages, is_kernel)) != NULL)
        return ret;
    else if (VMM_NODE_IS_EMPTY(parent) &&
        parent->u.empty_pages >= pages &&
        (( is_kernel && parent->base >= 0x80000000) ||
         (!is_kernel && parent->base <  0x80000000)))
        return parent;
    else if ((ret = VmmFindEmptyNode(parent->right, pages, is_kernel)) != NULL)
        return ret;
    else
        return NULL;
}

/*
 * Allocate a process-specific node for the given descriptor and try to fit it 
 *  in to the available address space
 */
vm_node_t *VmmAllocNode(process_t *proc, addr_t base, uint32_t flags, 
                        vm_desc_t *desc, bool is_kernel)
{
    vm_node_t *collide, *collide2, *node;

    assert(flags != 0);
    collide = NULL;
    base = PAGE_ALIGN(base);
    if (base == NULL && (flags & MEM_LITERAL) == 0)
    {
        /*
         * base == 0: Choose a base address
         */

        collide = VmmFindEmptyNode(proc->vmm_top, desc->num_pages, is_kernel);
        if (collide == NULL)
        {
            wprintf(L"VmmMap(%u, %x): out of address space\n", 
                desc->num_pages * PAGE_SIZE, base);
            free(desc);
            errno = ENOMEM;
            //__asm__("int3");
            assert(collide != NULL);
            return NULL;
        }

        assert(VMM_NODE_IS_EMPTY(collide));
        base = collide->base;
        collide->base += desc->num_pages * PAGE_SIZE;
        collide->u.empty_pages -= desc->num_pages;
    }
    else
    {
        /*
         * base != 0: Split empty nodes to accomodate
         */

        /*wprintf(L"VmmMap(%u, %x): desc_first = %p, size = %u, base = %x\n",
            pages * PAGE_SIZE,
            base,
            proc->desc_first,
            proc->desc_first->num_pages * PAGE_SIZE,
            proc->desc_first->base);*/

        collide = VmmLookupNode(proc->vmm_top, (void*) base);
        assert(collide != NULL);
        if (!VMM_NODE_IS_EMPTY(collide))
        {
            wprintf(L"VmmMap(%u, %x): collided with page at %x\n", 
                desc->num_pages * PAGE_SIZE, base, collide->base);
            free(desc);
            errno = EACCESS;
            return NULL;
        }

        if (collide->base == base)
        {
            /*
             * Allocating at the base of an empty region
             */

            collide->base += desc->num_pages * PAGE_SIZE;
            collide->u.empty_pages -= desc->num_pages;
        }
        else if (collide->base + collide->u.empty_pages * PAGE_SIZE 
            == base + desc->num_pages * PAGE_SIZE)
        {
            /*
             * Allocating at the end of an empty region
             */

            collide->u.empty_pages -= desc->num_pages;
        }
        else
        {
            /*
             * Allocating in the middle of an empty region
             */

            assert(collide->base + collide->u.empty_pages * PAGE_SIZE >
                (base + desc->num_pages * PAGE_SIZE));

            collide2 = malloc(sizeof(*collide2));
            memset(collide2, 0, sizeof(*collide2));
            collide2->base = base + desc->num_pages * PAGE_SIZE;
            collide2->flags = 0;
            collide2->u.empty_pages = 
                (collide->base + collide->u.empty_pages * PAGE_SIZE -
                (base + desc->num_pages * PAGE_SIZE)) / PAGE_SIZE;
            collide->u.empty_pages = (base - collide->base) / PAGE_SIZE;

            SpinAcquire(&proc->sem_vmm);
            //LIST_ADD(proc->desc, collide2);
            VmmInsertNode(proc->vmm_top, collide2);
            SpinRelease(&proc->sem_vmm);
        }
    }

    node = malloc(sizeof(*node));
    memset(node, 0, sizeof(*node));
    node->base = base;
    node->flags = flags;
    node->u.desc = desc;

    SpinAcquire(&proc->sem_vmm);
    VmmInsertNode(proc->vmm_top, node);
    SpinRelease(&proc->sem_vmm);

    return node;
}

void* VmmMap(size_t pages, addr_t start, void *dest1, void *dest2, 
             unsigned type, uint32_t flags)
{
    vm_desc_t *desc;
    vm_node_t *node;
    process_t *proc;

    proc = current()->process;
    desc = malloc(sizeof(*desc));
    if (!desc)
        return NULL;

    HndInit(&desc->hdr, 'vmmd');
    desc->pages = MemDupPageArray(pages, 0, NULL);
    desc->name = NULL;
    desc->shared_prev = desc->shared_next = NULL;
    desc->num_pages = pages;

    if (desc->pages == NULL)
    {
        free(desc);
        return false;
    }

    desc->type = type;
    SpinInit(&desc->mtx_allocate);
    SpinInit(&desc->spin);

    switch (type)
    {
    case VM_AREA_NORMAL:
        break;
    case VM_AREA_MAP:
        desc->dest.phys_map = (addr_t) dest1;
        break;
    case VM_AREA_FILE:
        desc->dest.file = (handle_t) dest1;
        break;
    case VM_AREA_IMAGE:
        desc->dest.mod = dest1;
        break;
    case VM_AREA_CALLBACK:
        desc->dest.callback.handler = dest1;
        desc->dest.callback.cookie = dest2;
        break;
    }

    node = VmmAllocNode(proc, start, flags, desc, 
        ((flags & 3) == 0) || (type == VM_AREA_IMAGE && start >= 0x80000000));
    if (node == NULL)
    {
        free(desc);
        return NULL;
    }
    else
        return (void*) node->base;
}

/*!    \brief Allocates an desc of memory in the specified process's address space.
*
*    The VmmArea() function can be used to get a vm_desc_t structure from
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
    return VmmMap(pages, start, NULL, NULL, VM_AREA_NORMAL, flags);
}

void* VmmAllocCallback(size_t pages, addr_t start, uint32_t flags, 
                       VMM_CALLBACK handler,
                       void *cookie)
{
    return VmmMap(pages, start, handler, cookie, VM_AREA_CALLBACK, flags);
}

void *VmmMapFile(handle_t file, addr_t start, size_t pages, uint32_t flags)
{
    flags &= ~(MEM_COMMIT | MEM_ZERO);
    return VmmMap(pages, start, (void*) file, NULL, VM_AREA_FILE, flags);
}

bool VmmShare(void *base, const wchar_t *name)
{
    vm_node_t *node;
    vm_desc_t *desc;
    wchar_t *name_copy;

    node = VmmLookupNode(current()->process->vmm_top, base);
    if (node == NULL ||
        VMM_NODE_IS_EMPTY(node))
    {
        errno = EINVALID;
        return false;
    }

    desc = node->u.desc;

    /*
     * xxx -- video driver stores shared area name in data section...
     *  we don't want to page it in while holding sem_vmm
     */
    name_copy = _wcsdup(name);
    SpinAcquire(&current()->process->sem_vmm);
    free(desc->name);
    desc->name = name_copy;

    SpinAcquire(&sem_share);

    if (shared_last != NULL)
        shared_last->shared_next = desc;
    desc->shared_prev = shared_last;
    desc->shared_next = NULL;
    shared_last = desc;
    if (shared_first == NULL)
        shared_first = desc;

    SpinRelease(&sem_share);

    SpinRelease(&current()->process->sem_vmm);
    return true;
}

handle_t VmmOpenSharedArea(const wchar_t *name)
{
    vm_desc_t *dest;

    SpinAcquire(&sem_share);

    for (dest = shared_first; dest != NULL; dest = dest->shared_next)
    {
        assert(dest->name != NULL);
        if (_wcsicmp(dest->name, name) == 0)
            break;
    }

    if (dest == NULL)
    {
        errno = ENOTFOUND;
        SpinRelease(&sem_share);
        return 0;
    }

    SpinRelease(&sem_share);

    return HndDuplicate(NULL, &dest->hdr);
}

void *VmmMapSharedArea(handle_t hnd, addr_t start, uint32_t flags)
{
    vm_desc_t *dest;
    void *ptr;
    vm_node_t *ret;

    ptr = HndLock(NULL, hnd, 'vmmd');
    if (ptr == NULL)
        return NULL;

    dest = (vm_desc_t*) ((handle_hdr_t*) ptr - 1);
    ret = VmmAllocNode(current()->process, start, flags, dest, (flags & 3) == 0);
    wprintf(L"VmmMapSharedArea(%s): start = %lx, base = %lx\n",
        dest->name, start, ret->base);
    HndUnlock(NULL, hnd, 'vmmd');
    return (void*) ret->base;
}

/*!    \brief Removes an desc of memory from the address space of the specified process.
*
*    The physical storage associated with the desc is deallocated and the desc is
*      un-mapped from the address space of the process.
*
*    \param    proc    The process which contains the desc
*    \param    desc    The desc to be uncommitted
*/
static void VmmUncommitNode(vm_node_t *node)
{
    int i;
    addr_t virt, phys;
    vm_desc_t *desc;

    //assert(desc->owner == current()->process);
    /*wprintf(L"vmmUncommit: %d => %x...", desc->pages, desc->start);*/

    assert(!VMM_NODE_IS_EMPTY(node));

    SpinAcquire(&current()->process->sem_vmm);
    virt = node->base;
    desc = node->u.desc;
    for (i = 0; i < desc->pages->num_pages; i++)
    {
        phys = desc->pages->pages[i];
        if (phys)
        {
            MemLockPages(phys, 1, false);
            if (!MemIsPageLocked(phys))
                MemFree(phys);
        }

        MemMap(virt, 0, virt + PAGE_SIZE, 0);
        virt += PAGE_SIZE;
    }

    SpinRelease(&current()->process->sem_vmm);

    /*wprintf(L"done\n"); */
}

/*!    \brief Frees an desc of memory allocated by the VmmAlloc() function.
*
*    \param    proc    The process which contains the desc
*    \param    desc    The desc of memory to be freed
*/
void VmmFree(const void *ptr)
{
    vm_node_t *node;
    vm_desc_t *desc;

    node = VmmLookupNode(current()->process->vmm_top, ptr);
    if (node == NULL ||
        VMM_NODE_IS_EMPTY(node))
        return;

    desc = node->u.desc;

    /*
     * xxx -- need to do this using handle semantics
     *  (i.e. unmap from process on VmmFree, free desc on handle cleanup)
     */

    /*wprintf(L"vmmFree: %d => %x...", desc->pages, desc->start);*/

    VmmUncommitNode(node);

    SpinAcquire(&current()->process->sem_vmm);
    node->flags = 0;
    node->u.empty_pages = desc->num_pages;
    SpinRelease(&current()->process->sem_vmm);

    SpinAcquire(&desc->spin);
    MemDeletePageArray(desc->pages);
    desc->pages = NULL;
    free(desc->name);
    desc->name = NULL;
    SpinRelease(&desc->spin);
    free(desc);

    /*free(desc);*/
    /*wprintf(L"done\n");*/
}

bool VmmMapAddressToFile(vm_node_t *node, vm_desc_t *desc, addr_t addr, 
                         uint64_t *off, size_t *bytes, uint32_t *flags)
{
    if (desc->type == VM_AREA_IMAGE)
    {
        /*
         * Relocated modules must have a different module structure for every
         *  different base address used
         */
        assert(node->base == desc->dest.mod->base);

        return PeMapAddressToFile(desc->dest.mod, addr, off, bytes, flags);
    }
    else
    {
        *off = addr - node->base;
        *bytes = PAGE_SIZE;
        *flags = node->flags;
        return true;
    }
}

bool VmmDoMapFile(vm_node_t *node, vm_desc_t *desc, addr_t start, bool is_writing)
{
    uint32_t flags;
    uint16_t state;
    addr_t phys, voff;
    uint64_t off;
    size_t bytes;
    handle_t file;
    process_t *proc;

    /*assert(desc->owner == current()->process || 
        desc->owner == &proc_idle);*/

    voff = start - node->base;
    state = MemGetPageState((const void*) start);
    proc = current()->process;
    if (proc->sem_vmm.locks != 0)
    {
        wprintf(L"VmmDoMapFile(%s, %x): lock eip = %p\n",
            proc->exe, 
            start, 
            proc->sem_vmm.owner);
        assert(proc->sem_vmm.locks == 0);
    }

    SpinAcquire(&proc->sem_vmm);
    //wprintf(L"VmmDoMapFile: start = %x ", start);

tryagain:
    switch (state)
    {
    case PAGE_PAGEDOUT:
        wprintf(L"PAGE_PAGEDOUT\n");

    case 0: /* not committed */
        /*
         * xxx - need to release VMM semaphore in case something needs to be 
         *  faulted in here?
         */
        SpinRelease(&proc->sem_vmm);
        //wprintf(L"VmmDoMapFile(%x): calling VmmMapAddressToFile...",
            //start);
        if (!VmmMapAddressToFile(node, desc, start, &off, &bytes, &flags))
        {
            SpinAcquire(&proc->sem_vmm);
            MemSetPageState((const void*) start, PAGE_READFAILED);
            SpinRelease(&proc->sem_vmm);
            return true;
        }

        //wprintf(L"done\n");
        /*wprintf(L"%s:%x: PAGE_PAGEDOUT off = %x bytes = %x flags = %x\n", 
            desc->owner->exe, start, (uint32_t) off, bytes, flags);*/

        SpinAcquire(&proc->sem_vmm);
        MtxAcquire(&desc->mtx_allocate);
        phys = MemAlloc();
        MemLockPages(phys, 1, true);

        SpinAcquire(&desc->spin);
        desc->pages->pages[voff / PAGE_SIZE] = phys;
        SpinRelease(&desc->spin);

        node->flags = (node->flags & ~(3 | MEM_READ | MEM_WRITE | MEM_ZERO)) 
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

                if (is_writing && (node->flags & MEM_WRITE))
                    state = PAGE_VALID_DIRTY;
                else
                    state = PAGE_VALID_CLEAN;

                if ((node->flags & 3) == 3)
                    state |= PRIV_USER;
                else
                    state |= PRIV_KERN;

                MemSetPageState((const void*) start, state);
                /*wprintf(L"%s:%x: finished blank page\n", desc->owner->exe, start);*/
            }

            MtxRelease(&desc->mtx_allocate);
            SpinRelease(&proc->sem_vmm);
            return true;
        }
        else
        {
            if (desc->type == VM_AREA_IMAGE)
                file = desc->dest.mod->file;
            else
                file = desc->dest.file;

            desc->pagingop.event = file;
            MemSetPageState((const void*) start, PAGE_READINPROG);

            desc->read_pages = MemDupPageArray(1, 0, &phys);
            FsSeek(file, off, FILE_SEEK_SET);
            if (!FsReadPhysical(file, desc->read_pages, bytes, &desc->pagingop))
                MemSetPageState((const void*) start, PAGE_READFAILED);

            MtxRelease(&desc->mtx_allocate);
            state = PAGE_READINPROG;
            goto tryagain;
        }

    case PAGE_VALID_CLEAN:
        /*wprintf(L"%s:%x: PAGE_VALID_CLEAN ", desc->owner->exe, start);*/
        if (node->flags & MEM_WRITE)
        {
            /*wprintf(L"=> PAGE_VALID_DIRTY\n");*/
            MemSetPageState((const void*) start, PAGE_VALID_DIRTY);
            SpinRelease(&proc->sem_vmm);
            return true;
        }

        break;

    case PAGE_VALID_DIRTY:
        wprintf(L"PAGE_VALID_DIRTY\n");
        break;

    case PAGE_READINPROG:
        //wprintf(L"PAGE_READINPROG\n");
        MtxAcquire(&desc->mtx_allocate);
        if (/*EvtIsSignalled(NULL, desc->pagingop.event) &&*/
            desc->pagingop.result != -1)
        {
            MemDeletePageArray(desc->read_pages);
            desc->read_pages = NULL;

            if (desc->pagingop.result == 0)
            {
                //wprintf(L"Read succeeded\n");
                if (is_writing && (node->flags & MEM_WRITE))
                    state = PAGE_VALID_DIRTY;
                else
                    state = PAGE_VALID_CLEAN;

                if ((node->flags & 3) == 3)
                    state |= PRIV_USER;
                else
                    state |= PRIV_KERN;

                SpinAcquire(&desc->spin);
                if (!MemMap(start, desc->pages->pages[voff / PAGE_SIZE], 
                    start + PAGE_SIZE, state))
                    MemSetPageState((const void*) start, PAGE_READFAILED);
                SpinRelease(&desc->spin);

                //wprintf(L"Finished\n");
                /*wprintf(L"%s:%x: finished page in, %u bytes read\n", 
                    desc->owner->exe, start, desc->pagingop.bytes);
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
                    start, desc->pagingop.result);
                MemSetPageState((const void*) start, PAGE_READFAILED);
            }
        }
        else
        {
            /*wprintf(L"PAGE_READINPROG(%x): waiting, result = %d\n", 
                start, desc->pagingop.result);*/
            ThrWaitHandle(current(), desc->pagingop.event, 0);
        }

        MtxRelease(&desc->mtx_allocate);
        SpinRelease(&proc->sem_vmm);

        if (desc->type == VM_AREA_IMAGE &&
            MemGetPageState((const void*) start) != PAGE_READINPROG /*&&
            !desc->dest.mod->imported*/)
            /*PeInitImage(desc->dest.mod);*/
            PeProcessSection(desc->dest.mod, start);
        return true;

    case PAGE_WRITEINPROG:
        wprintf(L"PAGE_WRITEINPROG\n");
        MtxAcquire(&desc->mtx_allocate);
        MtxRelease(&desc->mtx_allocate);
        SpinRelease(&proc->sem_vmm);
        return true;

    case PAGE_READFAILED:
        wprintf(L"PAGE_READFAILED\n");
        break;

    case PAGE_WRITEFAILED:
        wprintf(L"PAGE_WRITEFAILED\n");
        break;

    default:
        wprintf(L"%08x: unknown state for mapped desc: %x\n", start, state);
    }

    SpinRelease(&proc->sem_vmm);
    return false;
}

/*!    \brief Commits an desc of memory, by mapping and initialising it.
*
*    This function is called by VmmAlloc() if the MEM_COMMIT flag is specified.
*    Otherwise, it will be called when a page fault occurs in the desc.
*
*    Physical storage is allocated (unless the MEM_LITERAL flag is specified) and
*      it is mapped into the address space of the specified process. If the 
*      process is the one currently executing, that desc of the address space
*      is invalidated, allowing it to be accessed immediately. If the MEM_ZERO
*      flag was specified then the desc is zeroed; otherwise, the contents of
*      the desc are undefined.
*
*    \param    proc    The process which contains the desc
*    \param    desc    The desc to be committed
*    \return     true if the desc could be committed, false otherwise.
*/
bool VmmCommit(vm_node_t *node, vm_desc_t* desc, addr_t start, bool is_writing)
{
    uint32_t f;
    uint16_t state;
    addr_t phys;
    unsigned page;
    process_t *proc;

    //assert(desc->owner == current()->process || desc->owner == &proc_idle);

    state = MemGetPageState((const void*) start);
    proc = current()->process;
    SpinAcquire(&proc->sem_vmm);
    switch (state)
    {
    case 0: /* not committed */
        if (is_writing && (node->flags & MEM_WRITE))
            f = PAGE_VALID_DIRTY;
        else
            f = PAGE_VALID_CLEAN;

        if ((node->flags & 3) == 3)
            f |= PRIV_USER;
        else
            f |= PRIV_KERN;

        page = PAGE_ALIGN_UP(start - node->base) / PAGE_SIZE;
        if (node->flags & MEM_LITERAL)
            MemMap(start, start, start + PAGE_SIZE, f);
        else
        {
            if (desc->type == VM_AREA_MAP)
                phys = desc->dest.phys_map + (start - node->base);
            else
            {
                phys = MemAlloc();
                if (!phys)
                {
                    wprintf(L"VmmCommit: out of memory\n");
                    SpinRelease(&proc->sem_vmm);
                    return false;
                }
            }

            MemMap(start, phys, start + PAGE_SIZE, f);
        }

        SpinAcquire(&desc->spin);
        desc->pages->pages[page] = phys;
        SpinRelease(&desc->spin);
        MemLockPages(phys, 1, true);

        /* xxx -- need to check descriptor flags here */
        if (node->flags & MEM_ZERO)
            memset((void*) start, 0, PAGE_SIZE);

        SpinRelease(&proc->sem_vmm);
        return true;

    case PAGE_VALID_CLEAN:
        /*wprintf(L"%s:%x: PAGE_VALID_CLEAN ", desc->owner->exe, start);*/
        if (node->flags & MEM_WRITE)
        {
            /*wprintf(L"=> PAGE_VALID_DIRTY\n");*/
            MemSetPageState((const void*) start, PAGE_VALID_DIRTY);
            SpinRelease(&proc->sem_vmm);
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

    SpinRelease(&proc->sem_vmm);
    return false;
}

bool VmmPageFault(process_t *proc, addr_t page, bool is_writing)
{
    vm_node_t *node;
    vm_desc_t *desc;

    node = VmmLookupNode(proc->vmm_top, (void*) page);
    assert(node != NULL);
    if (VMM_NODE_IS_EMPTY(node))
        return false;

    page = PAGE_ALIGN(page);
    desc = node->u.desc;
    switch (desc->type)
    {
    case VM_AREA_NORMAL:
    case VM_AREA_MAP:
        return VmmCommit(node, desc, page, is_writing);

    case VM_AREA_FILE:
    case VM_AREA_IMAGE:
        return VmmDoMapFile(node, desc, page, is_writing);

    case VM_AREA_CALLBACK:
        return desc->dest.callback.handler(desc->dest.callback.cookie,
            node,
            page,
            is_writing);
    }

    return false;
}

void *morecore_user(size_t nbytes)
{
    return VmmAlloc(PAGE_ALIGN_UP(nbytes) / PAGE_SIZE, 
        NULL,
        3 | MEM_READ | MEM_WRITE);
}
