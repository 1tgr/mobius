/* $Id: vmm.c,v 1.3 2003/06/22 15:44:56 pavlovskii Exp $ */

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
#include <string.h>

extern process_t proc_idle;

vm_desc_t *shared_first, *shared_last;
spinlock_t sem_share;

/*
 * Finds the vm_node_t structure for a given virtual address
 */
vm_node_t *VmmLookupNode(int level, vm_node_t *parent, const void *ptr)
{
    addr_t base_pages;

	//assert(level < 10);
    base_pages = (addr_t) ptr / PAGE_SIZE;
    if (parent == NULL)
        return NULL;
    else if ((addr_t) ptr < parent->base)
        return VmmLookupNode(level + 1, parent->left, ptr);
    else if (VMM_NODE_IS_EMPTY(parent) && 
        base_pages < parent->base / PAGE_SIZE + parent->u.empty_pages)
        return parent;
    else if (!VMM_NODE_IS_EMPTY(parent) && 
        base_pages < parent->base / PAGE_SIZE + parent->u.desc->num_pages)
        return parent;
    else
        return VmmLookupNode(level + 1, parent->right, ptr);
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
    if (base == NULL && (flags & VM_MEM_LITERAL) == 0)
    {
        /*
         * base == 0: Choose a base address
         */

        collide = VmmFindEmptyNode(proc->vmm_top, desc->num_pages, is_kernel);
        if (collide == NULL)
        {
            wprintf(L"VmmMap(%u, %x): out of address space\n", 
                desc->num_pages * PAGE_SIZE, base);
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
         * base != 0: Split empty or reserved nodes to accomodate
         */

        /*wprintf(L"VmmMap(%u, %x): desc_first = %p, size = %u, base = %x\n",
            pages * PAGE_SIZE,
            base,
            proc->desc_first,
            proc->desc_first->num_pages * PAGE_SIZE,
            proc->desc_first->base);*/

        collide = VmmLookupNode(0, proc->vmm_top, (void*) base);
        assert(collide != NULL);

        /* We can split either empty or reserved nodes */
        if ((collide->flags & VM_MEM_RESERVED) == 0 &&
            !VMM_NODE_IS_EMPTY(collide))
        {
            wprintf(L"VmmMap(%x, %x): collided with page at %x, %x bytes\n", 
                desc->num_pages * PAGE_SIZE, base, collide->base, 
                VMM_NODE_IS_EMPTY(collide) 
                    ? collide->u.empty_pages * PAGE_SIZE
                    : collide->u.desc->num_pages * PAGE_SIZE);
			assert(false && "VmmMap: collision");
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

            /*
             * Make sure the new node has the same flags as the old one: zero
             *  if we're splitting an empty node, or VM_MEM_RESERVED if we're
             *  splitting a reserved node.
             */
            collide2->flags = collide->flags;
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
    node->flags = flags & VM_MEM_NODE_MASK;
    node->u.desc = desc;

    SpinAcquire(&proc->sem_vmm);
    VmmInsertNode(proc->vmm_top, node);
    SpinRelease(&proc->sem_vmm);

    return node;
}

static void VmmCleanupDesc(void *ptr)
{
	vm_desc_t *desc;
	unsigned i;
	addr_t phys;
	file_handle_t *file;

	desc = ptr;

    SpinAcquire(&desc->spin);

	for (i = 0; i < desc->num_pages; i++)
	{
		phys = desc->pages->pages[i];

		if (phys != NULL)
		{
			assert(MemIsPageLocked(phys) == 1);
			MemFree(phys);
		}
	}

    MemDeletePageArray(desc->pages);
    desc->pages = NULL;
    free(desc->name);
    desc->name = NULL;

	if (desc->type == VM_AREA_FILE)
		file = desc->dest.file;
	else
		file = NULL;

    SpinRelease(&desc->spin);
	free(desc);

	if (file != NULL)
		HndClose(&file->hdr);
}

/*!
 *  \brief  Creates any type of memory area in the current process
 *
 *  Called by \p VmmAlloc and \p VmmAllocCallback and \p VmmMapFile.
 *  This function creates the \p vm_desc_t structure backing the memory area,
 *      and calls \p VmmAllocNode to allocate space in the current process's
 *      address space. Other functions (\p VmmMapSharedArea) can attach 
 *      additional \p vm_node_t structures to the same \p vm_desc_t.
 *
 *  \param  pages   Size of the area to allocate, in pages
 *  \param  start   Start address of the area in the current process, or use 
 *      \p NULL to get \p VmmAllocNode to choose a suitable address. The 
 *      address chosen when \p NULL is passed will be below 2GB for user-mode
 *      allocations ((\p flags & 3) != 0) and above 2GB for kernel-mode
 *      allocations ((\p flags & 3) == 0).
 *  \param  dest1   Parameter for the area; this pointer is interpreted 
 *      differently according to the value passed for \p type.
 *  \param  dest2   Additional parameter for the area
 *  \param  type    Type of mapping to create. Can be one of the following
 *      values:
 *  - \p VM_AREA_NORMAL: \p dest1 and \p dest2 are ignored.
 *  - \p VM_AREA_MAP: \p dest1 specifies the physical address of a virtual-to-
 *      physical mapping; \p dest2 is ignored.
 *  - \p VM_AREA_FILE: \p dest1 specifies a file handle in the current process;
 *      \p dest2 is ignored.
 *  - \p VM_AREA_IMAGE: \p dest1 specifies a pointer to a \p module_t 
 *      structure; \p dest2 is ignored.
 *  - \p VM_AREA_CALLBACK: \p dest1 specifies a handler function whose 
 *      signature matches that of the typedef \p VMM_CALLBACK; \p dest2 
 *      specifies the cookie to be passed to the handler function.
 *  \param  flags   Flags to apply to the allocation and mapping of the area.
 *      Can be a bitwise-OR combination of the any of the following values:
 *  - \p VM_MEM_READ: Permits read access to the area. Note: current x86 
 *      processors always allow read access to memory.
 *  - \p VM_MEM_WRITE: Permits write access to the area. Note: on the x86, user-
 *      mode pages area always writable from kernel mode, regardless of 
 *      whether write protection has been applied.
 *  - \p VM_MEM_ZERO: Causes each page in the area to be zeroed as it is 
 *      allocated.
 *  \return A pointer to the start of the newly-allocated area, or \p NULL is
 *      allocation failed.
 */
void* VmmMap(size_t pages, addr_t start, void *dest1, void *dest2, 
             unsigned type, uint32_t flags)
{
    static handle_class_t descriptor_class =
        HANDLE_CLASS('vmmd', VmmCleanupDesc, NULL, NULL, NULL);
    vm_desc_t *desc;
    vm_node_t *node;
    process_t *proc;

    proc = current()->process;
    desc = malloc(sizeof(*desc));
    if (!desc)
        return NULL;

    HndInit(&desc->hdr, &descriptor_class);
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
        desc->dest.file = (file_handle_t*) HndCopy(dest1);
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

/*!
 *  \brief Allocates an area of memory
 *
 *  \param  pages   Number of 4KB pages to allocate
 *  \param  start   Virtual address at which the area will start. If this 
 *      is \p NULL an address will be chosen (unless \p flags includes the 
 *      \p VM_MEM_LITERAL flag).
 *  \param  flags   \p VM_MEM_xxx flags controlling the allocation of and access to 
 *      the allocated area
 *
 *  \return     A pointer to the start of the area, or \p NULL if the area could
 *      not be allocated.
 */
void* VmmAlloc(size_t pages, addr_t start, uint32_t flags)
{
    return VmmMap(pages, start, NULL, NULL, VM_AREA_NORMAL, flags);
}

/*!
 *  \brief  Allocates an area of memory whose mapping is controlled by a 
 *      callback function
 *
 *  \param  pages   Number of 4KB pages to allocate
 *  \param  start   Virtual address at which the area will start. If this
 *      if \p NULL an address will be chosen (unless \p flags includes the 
 *      \p VM_MEM_LITERAL flag).
 *  \param  flags   \p VM_MEM_xxx flags controlling the allocation of and access to
 *      the allocated area
 *  \param  handler Function called by the kernel when a page fault occurs in 
 *      the allocated area
 *  \param  cookie  Opaque pointer passed to \p handler
 *
 *  \return A pointer to the start of the area, or \p NULL if the area could
 *      not be allocated.
 */
void* VmmAllocCallback(size_t pages, addr_t start, uint32_t flags, 
                       VMM_CALLBACK handler,
                       void *cookie)
{
    return VmmMap(pages, start, handler, cookie, VM_AREA_CALLBACK, flags);
}

/*!
 *  \brief  Allocates an area of memory backed by a file
 *
 *  \param  file    Handle to the file to be mapped
 *  \param  start   Virtual address at which the area will start. If this
 *      if \p NULL an address will be chosen (unless \p flags includes the 
 *      \p VM_MEM_LITERAL flag).
 *  \param  pages   Number of 4KB pages to map
 *  \param  flags   \p VM_MEM_xxx flags controlling the allocation of and access to
 *      the allocated area
 *
 *  \return A pointer to the start of the area, or \p NULL if the area could
 *      not be mapped.
 */
void *VmmMapFile(file_handle_t *file, addr_t start, size_t pages, uint32_t flags)
{
    flags &= ~VM_MEM_ZERO;
    return VmmMap(pages, start, file, NULL, VM_AREA_FILE, flags);
}

/*!
 *  \brief  Makes an area of memory available for sharing through 
 *      \p VmmOpenSharedArea and \p VmmMapSharedArea
 *
 *  \param  base    Pointer to the start of the area to be shared
 *  \param  name    Name under which the area will be made available
 *
 *  \return \p true if the area could be shared, \p false otherwise
 */
vm_desc_t *VmmShare(void *base, const wchar_t *name)
{
    vm_node_t *node;

    node = VmmLookupNode(0, current()->process->vmm_top, base);
    if (node == NULL ||
        VMM_NODE_IS_EMPTY(node))
    {
        errno = EINVALID;
        return false;
    }

    HndExport(&node->u.desc->hdr, name);
    return node->u.desc;
}

void *VmmMapSharedArea(vm_desc_t *desc, addr_t start, uint32_t flags)
{
    vm_node_t *ret;

    ret = VmmAllocNode(current()->process, 
        start, 
        flags, 
        (vm_desc_t*) HndCopy(&desc->hdr), 
        (flags & 3) == 0);

	//wprintf(L"VmmMapSharedArea: base = %p, desc = %p\n", (void*) ret->base, dest);
    return (void*) ret->base;
}

void *VmmReserveArea(size_t pages, addr_t start)
{
    vm_node_t *ret;

    ret = VmmAllocNode(current()->process, start, VM_MEM_RESERVED, NULL, false);

    if (ret == NULL)
        return NULL;
    else
        return (void*) ret->base;
}


void VmmFreeNode(vm_node_t *node)
{
    vm_desc_t *desc;
	process_t *proc;
	uint8_t *virt, *end;
	unsigned i;

    desc = node->u.desc;
	proc = current()->process;

    SpinAcquire(&proc->sem_vmm);

	virt = (uint8_t*) node->base;
	end = virt + desc->pages->num_pages * PAGE_SIZE;
	i = 0;
	while (virt < end)
	{
		if (desc->pages->pages[i] != NULL)
			MemMapRange(virt,
				0,
				virt + PAGE_SIZE,
				0);

		virt += PAGE_SIZE;
		i++;
	}

    node->flags = 0;
    node->u.empty_pages = desc->num_pages;
    SpinRelease(&proc->sem_vmm);

    HndClose(&desc->hdr);
}


/*!
 *  \brief Frees an area of memory allocated by the VmmAlloc() function.
 *
 *  \param  ptr Pointer with the area to be freed
 */
bool VmmFree(void *ptr)
{
    vm_node_t *node;

    node = VmmLookupNode(0, current()->process->vmm_top, ptr);
    if (node == NULL ||
        VMM_NODE_IS_EMPTY(node))
        return false;

    VmmFreeNode(node);
    return true;
}

static bool VmmMapAddressToFile(vm_node_t *node, vm_desc_t *desc, 
                                addr_t addr, uint64_t *off, size_t *bytes, 
                                uint32_t *flags)
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

static bool VmmDoMapFile(vm_node_t *node, vm_desc_t *desc, addr_t start, 
                         bool is_writing)
{
    uint32_t flags;
    uint16_t state;
    addr_t phys, voff;
    uint64_t off;
    size_t bytes;
    file_handle_t *file;
    process_t *proc;
	status_t ret;

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

tryagain:
    switch (state)
    {
    case PAGE_PAGEDOUT:
        wprintf(L"PAGE_PAGEDOUT\n");

    case 0: /* not committed */

        /* xxx - need to release VMM semaphore in case something needs to be 
         *  faulted in here? */
        SpinRelease(&proc->sem_vmm);
        if (!VmmMapAddressToFile(node, desc, start, &off, &bytes, &flags))
        {
            SpinAcquire(&proc->sem_vmm);
            MemSetPageState((const void*) start, PAGE_READFAILED);
            SpinRelease(&proc->sem_vmm);
            return true;
        }

        SpinAcquire(&proc->sem_vmm);
        MtxAcquire(&desc->mtx_allocate);
        phys = MemAlloc();
        assert(phys != 0);
        MemLockPages(phys, 1, true);

        SpinAcquire(&desc->spin);
        desc->pages->pages[voff / PAGE_SIZE] = phys;
        SpinRelease(&desc->spin);

        /*
         * Allow VmmMapAddressToFile to set read, write, zero and PL bits;
         *  apply other bits from node.
         */
        flags = (node->flags & ~(3 | VM_MEM_READ | VM_MEM_WRITE | VM_MEM_ZERO)) 
              | (flags       &  (3 | VM_MEM_READ | VM_MEM_WRITE | VM_MEM_ZERO));

        /*wprintf(L"%08x: ", node->base);
        if (flags & VM_MEM_READ)
            wprintf(L"readable ");
        if (flags & VM_MEM_WRITE)
            wprintf(L"writeable ");
        if (flags & VM_MEM_ZERO)
            wprintf(L"zeroed ");
        wprintf(L"\n");*/

        if (off == (uint64_t) -1 ||
            bytes == (size_t) -1)
        {
            if (is_writing || (flags & VM_MEM_WRITE))
                state = PAGE_VALID_DIRTY;
            else
                state = PAGE_VALID_CLEAN;

            if ((flags & 3) == 3)
                state |= PRIV_USER;
            else
                state |= PRIV_KERN;

            if (MemMapRange((void*) start, phys, (uint8_t*) start + PAGE_SIZE, state))
                memset((void*) start, 0, PAGE_SIZE);
            else
                MemSetPageState((void*) start, PAGE_READFAILED);

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

            desc->pagingop.event = current()->vmm_io_event;
            MemSetPageState((const void*) start, PAGE_READINPROG);

            desc->read_pages = MemDupPageArray(1, 0, &phys);
            SpinRelease(&proc->sem_vmm);
	    ret = FsReadPhysicalAsync(file, desc->read_pages, off, bytes, &desc->pagingop);
	    if (ret > 0)
	    {
                SpinAcquire(&proc->sem_vmm);
	        wprintf(L"VmmDoMapFile: read failed: %d\n", ret);
                MemSetPageState((const void*) start, PAGE_READFAILED);
		SpinRelease(&proc->sem_vmm);
		return false;
	    }
	    else if (ret == SIOPENDING)
	    {
		handle_hdr_t *event;

		event = HndRef(proc, current()->vmm_io_event, 'evnt');
		assert(event != NULL);
		event = HndCopy(event);
		ThrWaitHandle(current(), event);
		HndClose(event);

		MtxRelease(&desc->mtx_allocate);

		SpinAcquire(&proc->sem_vmm);
		state = PAGE_READINPROG;
		goto tryagain;
	    }
	    else
	    {
                SpinAcquire(&proc->sem_vmm);
		desc->pagingop.result = 0;
		MtxRelease(&desc->mtx_allocate);
		state = PAGE_READINPROG;
		goto tryagain;
	    }
        }

    case PAGE_VALID_CLEAN:
        SpinRelease(&proc->sem_vmm);
        if (!VmmMapAddressToFile(node, desc, start, &off, &bytes, &flags))
        {
            SpinRelease(&proc->sem_vmm);
            return false;
        }

        /*
         * Need to check what the flags in the file say, not what the flags 
         *  in the node say
         */
        if (flags & VM_MEM_WRITE)
        {
            MemSetPageState((const void*) start, PAGE_VALID_DIRTY);
            SpinRelease(&proc->sem_vmm);
            return true;
        }

        break;

    case PAGE_VALID_DIRTY:
        wprintf(L"PAGE_VALID_DIRTY\n");
        break;

    case PAGE_READINPROG:
        MtxAcquire(&desc->mtx_allocate);
        if (/*EvtIsSignalled(NULL, desc->pagingop.event) &&*/
            desc->pagingop.result != -1)
        {
            MemDeletePageArray(desc->read_pages);
            desc->read_pages = NULL;

            if (desc->pagingop.result == 0 &&
		VmmMapAddressToFile(node, desc, start, &off, &bytes, &flags))
            {
		/*wprintf(L"finished read %08x: ", node->base);
		if (flags & VM_MEM_READ)
			wprintf(L"readable ");
		if (flags & VM_MEM_WRITE)
			wprintf(L"writeable ");
		if (flags & VM_MEM_ZERO)
			wprintf(L"zeroed ");
		wprintf(L"\n");*/

                if (is_writing || (flags & VM_MEM_WRITE))
                    state = PAGE_VALID_DIRTY;
                else
                    state = PAGE_VALID_CLEAN;

                if ((flags & 3) == 3)
                    state |= PRIV_USER;
                else
                    state |= PRIV_KERN;

                SpinAcquire(&desc->spin);
                if (!MemMapRange((void*) start, desc->pages->pages[voff / PAGE_SIZE], 
                    (uint8_t*) start + PAGE_SIZE, state))
                    MemSetPageState((void*) start, PAGE_READFAILED);
                SpinRelease(&desc->spin);
            }
            else
            {
                wprintf(L"PAGE_READINPROG(%x): read failed: %d\n", 
                    start, desc->pagingop.result);
                MemSetPageState((void*) start, PAGE_READFAILED);
            }
        }
        else
	{
	    handle_hdr_t *event;
	    event = HndRef(NULL, desc->pagingop.event, 0);
            ThrWaitHandle(current(), event);
	}

        MtxRelease(&desc->mtx_allocate);
        SpinRelease(&proc->sem_vmm);

        if (desc->type == VM_AREA_IMAGE &&
            MemGetPageState((const void*) start) != PAGE_READINPROG /*&&
            !desc->dest.mod->imported*/)
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

/*!
 *  \brief Brings a normal or mapped page into physical memory
 *
 *  Called by \p VmmPageFault for \p VM_AREA_NORMAL and \p VM_AREA_MAP areas.
 *
 *  \param  node    Node identifying the area to be paged in the current 
 *      process
 *  \param  desc    Descriptor backing the memory area
 *  \param  start   Virtual address which caused the page fault
 *  \param  is_writing  Set to \p true if the access to address \p start was 
 *      a write, of \p false for a read
 *  \return \p true if the function could handle the page fault, \p false 
 *      otherwise
 */
bool VmmCommit(vm_node_t *node, vm_desc_t* desc, addr_t start, bool is_writing)
{
    uint32_t f;
    uint16_t state;
    addr_t phys;
    unsigned page;
    process_t *proc;
	bool is_zeroed;

    //assert(desc->owner == current()->process || desc->owner == &proc_idle);

    state = MemGetPageState((const void*) start);
    proc = current()->process;
    SpinAcquire(&proc->sem_vmm);
    switch (state)
    {
    case 0: /* not committed */
        if (is_writing && (node->flags & VM_MEM_WRITE))
            f = PAGE_VALID_DIRTY;
        else
            f = PAGE_VALID_CLEAN;

        if ((node->flags & 3) == 3)
            f |= PRIV_USER;
        else
            f |= PRIV_KERN;

        page = PAGE_ALIGN_UP(start - node->base) / PAGE_SIZE;
		is_zeroed = false;
        if (node->flags & VM_MEM_LITERAL)
		{
			wprintf(L"VmmCommit: VM_MEM_LITERAL, %08x\n", start);
            phys = start;
		}
        else if (desc->pages->pages[page] == NULL)
        {
            if (desc->type == VM_AREA_MAP)
                phys = desc->dest.phys_map + (start - node->base);
            else
            {
                if (desc->flags & VM_MEM_ZERO)
                {
                    /*while ((phys = MemAllocFromList(&mem_zero)) == NULL)
                    {
                        wprintf(L"VmmCommit: out of zeroed pages\r");
                        KeYield();
                    }*/

					phys = MemAllocFromList(&mem_zero);
					if (phys == NULL)
						phys = MemAlloc();
					else
						is_zeroed = true;

                    wprintf(L"VmmCommit: got zeroed page %x\n", phys);
                }
                else
                    phys = MemAlloc();

                if (!phys)
                {
                    wprintf(L"VmmCommit: out of memory\n");
                    SpinRelease(&proc->sem_vmm);
                    return false;
                }
            }

            SpinAcquire(&desc->spin);
			desc->pages->pages[page] = phys;
			SpinRelease(&desc->spin);

			MemLockPages(phys, 1, true);
        }
		else
			phys = desc->pages->pages[page];

        /* xxx -- need to check descriptor flags here */
        //if (node->flags & VM_MEM_ZERO)
            //memset((void*) start, 0, PAGE_SIZE);

		MemMapRange((void*) start, phys, (uint8_t*) start + PAGE_SIZE, f);

		if (node->flags & VM_MEM_ZERO)
		{
			uint8_t *ptr;

			if (!is_zeroed)
				memset((void*) start, 0, PAGE_SIZE);

            for (ptr = (uint8_t*) start; ptr < (uint8_t*) start + PAGE_SIZE; ptr++)
                if (*ptr != '\0')
                {
                    wprintf(L"VmmCommit: zeroed page failed at %lx/%lx: %02x\n",
                        (addr_t) ptr,
                        start,
                        *ptr);
                }
		}

        SpinRelease(&proc->sem_vmm);
        return true;

    case PAGE_VALID_CLEAN:
        /*wprintf(L"%s:%x: PAGE_VALID_CLEAN ", desc->owner->exe, start);*/
        if (node->flags & VM_MEM_WRITE)
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

/*!
 *  \brief  Handles a page fault in VMM-managed memory in the current process
 *
 *  Attempts to look up the fault address in the current process's VMM nodes 
 *      and, if found, calls the appropriate handler function to bring the
 *      page into physical memory.
 *
 *  \param  proc    Process whose nodes are to be checked. This can be either 
 *      \p current()->\p process or \p proc_idle.
 *  \param  page    Virtual address which caused the page fault
 *  \param  is_writing  Set to \p true if the access to address \p page was 
 *      a write, of \p false for a read
 *  \return \p true if the function could handle the page fault, \p false 
 *      otherwise
 */
bool VmmPageFault(process_t *proc, addr_t page, bool is_writing)
{
    vm_node_t *node;
    vm_desc_t *desc;

	if (page >= 0x80000000 && is_writing)
	{
		wprintf(L"VmmPageFault(%s, %08x, true): write fault in kernel area\n",
			proc->exe, page);
		return false;
	}

    node = VmmLookupNode(0, proc->vmm_top, (void*) page);
    if (node == NULL || VMM_NODE_IS_EMPTY(node))
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
        VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
}
