/* $Id: memory.c,v 1.19 2002/09/13 23:06:40 pavlovskii Exp $ */
#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/init.h>
#include <kernel/multiboot.h>

#include <stdio.h>
#include <string.h>

#define MEM_TEMP_START	  0xf8000000

extern addr_t kernel_pagedir[];
extern char scode[], ebss[], edata[], _data_start__[];
extern process_t proc_idle;

page_phys_t *mem_pages;
pfn_list_t mem_free =     { { 0 }, PFN_NULL, PFN_NULL, 0 };
pfn_list_t mem_free_low = { { 0 }, PFN_NULL, PFN_NULL, 0 };
pfn_list_t mem_zero =     { { 0 }, PFN_NULL, PFN_NULL, 0 };
addr_t mem_temp_end = MEM_TEMP_START;
uint8_t *locked_pages;

bool mem_ready;

/*
 * Mike Rieker's page states, i386-style:
 *
 * Mask = 111110011011 = 0xf9b
 * PAGEDOUT, page is set to 'no-access'     AVAIL=000 RW=0 P=0 00000xx00x00 = 0x000
 * READINPROG, page is set to 'no-access'   AVAIL=001 RW=0 P=0 00100xx00x00 = 0x200
 * WRITEINPROG, page is set to 'read-only'  AVAIL=010 RW=0 P=1 01000xx00x01 = 0x401
 * READFAILED, page is set to 'no-access'   AVAIL=011 RW=0 P=0 01100xx00x00 = 0x600
 * WRITEFAILED, page is set to 'read-only'  AVAIL=100 RW=0 P=1 10000xx00x01 = 0x801
 * VALID_CLEAN, page is set to 'read-only'  AVAIL=101 RW=0 P=1 10100xx00x01 = 0xa01
 * VALID_DIRTY, page is set to 'read/write' AVAIL=110 RW=1 P=1 11000xx00x11 = 0xc02
 */

page_phys_t *MemLookupAllocatedPage(struct vm_desc_t *desc, addr_t offset)
{
    return NULL;
}

static void MemAddToList(pfn_list_t *list, addr_t addr, uint16_t flags)
{
    page_phys_t *page;
    pfn_t pfn;

    SpinAcquire(&list->spin);
    pfn = PHYS_TO_PFN(addr);
    page = mem_pages + pfn;
    page->flags = flags;

    if (list->last != PFN_NULL)
        mem_pages[list->last].u.list.next = pfn;
    page->u.list.prev = list->last;
    page->u.list.next = PFN_NULL;
    list->last = pfn;
    if (list->first == PFN_NULL)
        list->first = pfn;
    list->num_pages++;

    SpinRelease(&list->spin);
}

static bool MemRemoveFromList(pfn_list_t *list, addr_t addr)
{
    page_phys_t *page;
    pfn_t pfn;

    SpinAcquire(&list->spin);
    if (list->first == PFN_NULL)
    {
        SpinRelease(&list->spin);
        return false;
    }

    pfn = PHYS_TO_PFN(addr);
    page = mem_pages + pfn;

    if (page->u.list.next != PFN_NULL)
        mem_pages[page->u.list.next].u.list.prev = page->u.list.prev;
    if (page->u.list.prev != PFN_NULL)
        mem_pages[page->u.list.prev].u.list.next = page->u.list.next;
    if (list->first == pfn)
        list->first = page->u.list.next;
    if (list->last == pfn)
        list->last = page->u.list.prev;
    page->u.list.next = page->u.list.prev = PFN_NULL;
    page->flags = (page->flags & ~PAGE_STATE_MASK) | PAGE_STATE_ALLOCATED;
    page->u.alloc_vm.desc = NULL;
    page->u.alloc_vm.offset = 0;
    list->num_pages--;

    SpinRelease(&list->spin);
    return true;
}

addr_t MemAllocFromList(pfn_list_t *list)
{
    addr_t ret;
    ret = PFN_TO_PHYS(list->first);
    if (!MemRemoveFromList(list, ret))
        return NULL;
    else
        return ret;
}

static void MemFreeRange(pfn_list_t *list, addr_t start, addr_t end, uint16_t flags)
{
    start = PAGE_ALIGN(start);
    end = PAGE_ALIGN(end);
    for (; start < end; start += PAGE_SIZE)
        MemAddToList(list, start, flags | PAGE_STATE_FREE);
}

addr_t MemAlloc(void)
{
    addr_t ret;
    ret = MemAllocFromList(&mem_zero);
    if (ret == NULL)
        ret = MemAllocFromList(&mem_free);
    return ret;
}

void MemFree(addr_t block)
{
    MemAddToList(&mem_free, block, PAGE_STATE_FREE);
}

addr_t MemAllocLow(void)
{
    return MemAllocFromList(&mem_free_low);
}

void MemFreeLow(addr_t block)
{
    MemAddToList(&mem_free_low, block, PAGE_STATE_FREE | PAGE_FLAG_LOW);
}

bool MemMap(addr_t virt, addr_t phys, addr_t virt_end, uint16_t priv)
{
    addr_t *pde;

    virt = PAGE_ALIGN(virt);
    phys = PAGE_ALIGN(phys);
    virt_end = PAGE_ALIGN(virt_end);

    for (; virt < virt_end; virt += PAGE_SIZE, phys += PAGE_SIZE)
    {
        pde = ADDR_TO_PDE(virt);

        if (*pde == 0)
        {
            addr_t pt;

            pt = MemAlloc();

            if (pt == NULL)
                return false;

            if (virt >= 0x80000000)
            {
                pt |= PAGE_GLOBAL;
                kernel_pagedir[PAGE_DIRENT(virt)] = pt 
                    | PRIV_WR | PRIV_USER | PRIV_PRES;
            }

            *pde = pt | PRIV_WR | PRIV_USER | PRIV_PRES;
        }

        *ADDR_TO_PTE(virt) = phys | priv;
        __asm__("invlpg %0"
            :
        : "m" (virt));
    }

    return true;
}

bool MemSetPageState(const void *virt, uint16_t state)
{
    addr_t v, *pde;

    v = PAGE_ALIGN((addr_t) virt);
    pde = ADDR_TO_PDE(virt);

    if (*pde == NULL)
    {
        addr_t pt;

        pt = MemAlloc();

        if (pt == NULL)
            return false;

        *pde = pt | PRIV_WR | PRIV_USER | PRIV_PRES;
    }

    *ADDR_TO_PTE(v) = (*ADDR_TO_PTE(v) & ~PAGE_STATEMASK) | state;
    __asm__("invlpg %0"
        :
        : "m" (v));

    return true;
}

uint32_t MemTranslate(const void *address)
{
    uint32_t ret;

    do
    {
        if (*ADDR_TO_PDE((addr_t) address))
            ret = *ADDR_TO_PTE((addr_t) address);
        else
            ret = 0;

        /* If page is not present, try to simulate a page fault (safely) */
        if ((ret & PRIV_PRES) == 0)
        {
            if (!i386HandlePageFault((addr_t) address, false))
                return 0;
        }
        else
            break;
    } while (true);

    return ret;
}

uint16_t MemGetPageState(const void *address)
{
    if (*ADDR_TO_PDE((addr_t) address))
        return *ADDR_TO_PTE((addr_t) address) & PAGE_STATEMASK;
    else
        return 0;
}

void *MemMapTemp(const addr_t *phys, unsigned num_pages, uint8_t priv)
{
    unsigned i;
    void *ptr;
    bool can_map_direct;

    can_map_direct = true;
    for (i = 0; i < num_pages; i++)
    {
        if ((phys[i] >= 0x08000000) ||
            (i > 0 && phys[i] != phys[i - 1] + PAGE_SIZE))
        {
            can_map_direct = false;
            break;
        }
    }

    if (can_map_direct)
        return PHYSICAL(*phys);

    ptr = (void*) mem_temp_end;
    for (i = 0; i < num_pages; i++)
    {
        if (!MemMap(mem_temp_end, phys[i], mem_temp_end + PAGE_SIZE, priv))
            return NULL;

        mem_temp_end += PAGE_SIZE;
    }

    MemMap(mem_temp_end, 0, mem_temp_end + PAGE_SIZE, 0);
    mem_temp_end += PAGE_SIZE;

    return ptr;
}

void MemUnmapTemp(void)
{
    MemMap(MEM_TEMP_START, 0, mem_temp_end, 0);
    mem_temp_end = MEM_TEMP_START;
}

addr_t MemAllocPageDir(void)
{
    addr_t page_dir, *pd;

    page_dir = MemAlloc();
    pd = MemMapTemp(&page_dir, 1, PRIV_KERN | PRIV_WR | PRIV_PRES);
    memset(pd, 0, 512 * sizeof(*pd));
    memcpy(pd + 512, kernel_pagedir + 512, 511 * sizeof(*pd));
    pd[1023] = page_dir | PRIV_KERN | PRIV_WR | PRIV_PRES;
    MemUnmapTemp();

    return page_dir;
}

bool MemVerifyBuffer(const void *buf, size_t bytes)
{
    addr_t virt;

    virt = PAGE_ALIGN((addr_t) buf);
    bytes = PAGE_ALIGN_UP(bytes);
    for (; bytes > 0; bytes -= PAGE_SIZE, virt += PAGE_SIZE)
        if (MemTranslate((const void*) virt) == NULL)
            return false;

        return true;
}

bool MemLockPages(addr_t phys, unsigned pages, bool do_lock)
{
    unsigned index;
    int d;

    phys = PAGE_ALIGN(phys);
    index = phys / PAGE_SIZE;
    d = do_lock ? 1 : -1;

    for (; pages > 0; pages--, index++, phys += PAGE_SIZE)
    {
        /*
        * xxx - drivers could try to lock memory outside of RAM (e.g. the 
        *  video frame buffer).
        * The kernel isn't going to do anything with these pages anyway, so
        *  just ignore attempts to lock/unlock them.
        */
        if (phys < kernel_startup.memory_size)
            locked_pages[index] += d;
    }

    return true;
}

bool MemIsPageLocked(addr_t phys)
{
    return locked_pages[PAGE_ALIGN_UP(phys) / PAGE_SIZE] > 0;
}

page_array_t *MemCopyPageArray(page_array_t *a)
{
    KeAtomicInc(&a->refs);
    return a;
}

page_array_t *MemDupPageArray(unsigned num_pages, size_t mod_first_page, 
                              const addr_t *pages)
{
    page_array_t *array;
    unsigned i;

    array = malloc(sizeof(page_array_t) + sizeof(addr_t) * (num_pages - 1));
    if (array == NULL)
        return NULL;

    array->refs = 0;
    array->num_pages = num_pages;
    array->mod_first_page = mod_first_page;

    for (i = 0; i < num_pages; i++)
    {
        if (pages == NULL)
            array->pages[i] = NULL;
        else
        {
            array->pages[i] = pages[i];
            MemLockPages(pages[i], 1, true);
        }
    }

    return array;
}

#define ADDRESS_AND_SIZE_TO_SPAN_PAGES(Va,Size) \
    (((((Size) - 1) / PAGE_SIZE) + \
    (((((uint32_t)(Size-1)&(PAGE_SIZE-1)) + ((addr_t) (Va) & (PAGE_SIZE -1)))) / PAGE_SIZE)) + 1L)

page_array_t *MemCreatePageArray(const void *buf, size_t bytes)
{
    page_array_t *array;
    unsigned num_pages, mod_first_page;
    addr_t *ptr, user_addr, phys;

    num_pages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(buf, bytes);
    user_addr = PAGE_ALIGN((addr_t) buf);
    mod_first_page = (addr_t) buf - user_addr;

    array = malloc(sizeof(page_array_t) + sizeof(addr_t) * (num_pages - 1));
    if (array == NULL)
        return NULL;

    array->refs = 0;
    array->mod_first_page = mod_first_page;
    array->num_pages = num_pages;
    for (ptr = array->pages; num_pages > 0; num_pages--, user_addr += PAGE_SIZE)
    {
        phys = MemTranslate((void*) user_addr) & -PAGE_SIZE;
        if (phys == NULL)
        {
            wprintf(L"MemCreatePageArray: buffer = %p: virt = %x is not mapped\n", 
                buf, user_addr);
            assert(phys != NULL);
        }

        MemLockPages(phys, 1, true);
        *ptr++ = phys;
    }

    return array;
}

void MemDeletePageArray(page_array_t *array)
{
    addr_t *ptr;
    unsigned i;

    if (array->refs == 0)
    {
        ptr = array->pages;
        for (i = 0; i < array->num_pages; i++, ptr++)
            MemLockPages(*ptr, 1, false);

        free(array);
    }
    else
        KeAtomicDec(&array->refs);
}

void *MemMapPageArray(page_array_t *array, uint16_t state)
{
    uint8_t *ptr;
    ptr = MemMapTemp(array->pages, array->num_pages, state);
    if (ptr == NULL)
        return NULL;
    else
        return ptr + array->mod_first_page;
}

void MemZeroPageThread(void)
{
    addr_t addr;
    void *ptr;

    while (true)
    {
        addr = MemAllocFromList(&mem_free);

        if (addr == NULL)
        {
            ThrSleep(current(), 2000);
            KeYield();
            continue;
        }

        //wprintf(L"MemZeroPageThread: page = %08x\n", addr);
        ptr = MemMapTemp(&addr, 1, PRIV_PRES | PRIV_RD | PRIV_WR);
        memset(ptr, 0, PAGE_SIZE);
        MemUnmapTemp();
        MemAddToList(&mem_zero, addr, PAGE_STATE_ZERO);
    }
}

/*!
 * \brief Initializes the physical memory manager
 *
 *    This is the first function called by \p KernelMain. Its purpose is to get
 *    the CPU into an environment suitable for running the rest of the kernel,
 *    and performs the following tasks:
 *
 *    - Sets aside physical page pools for main and low memory
 *    - Distinguishes between usable and unusable memory
 *    - Sets up the initial kernel page directory
 *    - Enables paging
 *    - Maps the bottom 128MB of memory into the region at 0xF0000000 
 *	  (= \p PHYSMEM )
 *
 *    \return	 \p true
 */
bool MemInit(void)
{
    unsigned num_pages;
    addr_t pt1, pt2, phys;
    uint32_t entry;
    size_t kernel_code;
    unsigned i;
    /*memory_map_t *map;*/
    multiboot_module_t *mod;

    num_pages = kernel_startup.memory_size / PAGE_SIZE;

    /*
     * Total kernel size composed of:
     *	  Kernel file image
     *	  bss
     *	  Page address stack
     *	  Page lock counts
     */
    kernel_startup.kernel_data = kernel_startup.kernel_size + ebss - edata;
    kernel_startup.kernel_data = PAGE_ALIGN_UP(kernel_startup.kernel_data);

    /* One page of PTEs at one page per PTE = 4MB */
    assert(kernel_startup.kernel_data < PAGE_SIZE * PAGE_SIZE / sizeof(uint32_t));

    mod = (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
    phys = mod[kernel_startup.multiboot_info->mods_count - 1].mod_end;
    phys = PAGE_ALIGN_UP(phys);

    /* Page address stack goes after the bss */
    mem_pages = DEMANGLE_PTR(page_phys_t*, phys);

    /* Page lock counts go after the stack */
    locked_pages = (uint8_t*) (mem_pages + num_pages);

    /* xxx - why does memset not work here? */
    for (i = 0; i < num_pages; i++)
        locked_pages[i] = 0;

    /* Memory from BDA=>ROMs */
    entry = 0x5000;
    assert(entry <= 0xA0000);
    MemFreeRange(&mem_free_low, entry, 0xA0000, PAGE_FLAG_LOW);
    /* Memory from top of ROMs to top of 'low memory' */
    MemFreeRange(&mem_free_low, 0x100000, LOW_MEMORY, PAGE_FLAG_LOW);

    MemLockPages(kernel_startup.kernel_phys, 
        PAGE_ALIGN_UP(kernel_startup.kernel_data) / PAGE_SIZE, 
        true);
    MemLockPages(phys, 
        PAGE_ALIGN_UP(num_pages * sizeof(page_phys_t) + num_pages) / PAGE_SIZE,
        true);
    mod = (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
    for (i = 0; i < kernel_startup.multiboot_info->mods_count; i++)
        MemLockPages(mod[i].mod_start, 
            PAGE_ALIGN_UP(mod[i].mod_end - mod[i].mod_start) / PAGE_SIZE,
            true);

    /*if (kernel_startup.multiboot_info->mmap_length > 0)
    {
        map = (memory_map_t*) kernel_startup.multiboot_info->mmap_addr;
        while ((uint32_t) map < kernel_startup.multiboot_info->mmap_addr + 
            kernel_startup.multiboot_info->mmap_length)
        {
            if (map->type != 1 &&
            map->length_low != 0 &&
            map->base_addr_high == 0)
            MemLockPages(map->base_addr_low, 
            PAGE_ALIGN_UP(map->length_low) / PAGE_SIZE, true);

            map = (memory_map_t*) ((uint8_t*) map + ((uint32_t*) map)[-1]);
        }
    }*/

    for (i = num_pages - 1; i >= NUM_LOW_PAGES; i--)
        if (locked_pages[i] == 0)
            MemAddToList(&mem_free, i * PAGE_SIZE, PAGE_STATE_FREE);

    /*
     * Allocate one page table and patch the entries in the page directory:
     *   - at 0xC0000000 for scode
     *   - at kernel_phys for identity mapping
     */

    pt1 = MemAlloc();
    kernel_pagedir[PAGE_DIRENT((addr_t) scode)] = pt1 | PRIV_WR | PRIV_USER | PRIV_PRES;

    pt2 = MemAlloc();
    kernel_pagedir[PAGE_DIRENT(kernel_startup.kernel_phys)] = pt2 | PRIV_WR | PRIV_USER | PRIV_PRES;

    kernel_code = _data_start__ - scode;

    /* Map code read-only */
    for (phys = 0; phys < kernel_code; phys += PAGE_SIZE)
    {
        entry = PAGE_TABENT((addr_t) scode + phys);
        i386_lpoke32(pt1 + entry * 4, 
            (kernel_startup.kernel_phys + phys) | PRIV_KERN | PRIV_PRES);

        entry = PAGE_TABENT((addr_t) kernel_startup.kernel_phys + phys);
        i386_lpoke32(pt2 + entry * 4, 
            (kernel_startup.kernel_phys + phys) | PRIV_KERN | PRIV_PRES);
    }

    /* Map data read-write */
    for ( ; phys < kernel_startup.kernel_data; phys += PAGE_SIZE)
    {
        entry = PAGE_TABENT((addr_t) scode + phys);
        i386_lpoke32(pt1 + entry * 4, 
            (kernel_startup.kernel_phys + phys) | PRIV_KERN | PRIV_WR | PRIV_PRES);

        entry = PAGE_TABENT((addr_t) kernel_startup.kernel_phys + phys);
        i386_lpoke32(pt2 + entry * 4, 
            (kernel_startup.kernel_phys + phys) | PRIV_KERN | PRIV_WR | PRIV_PRES);
    }

    /* Calculate physical address of kernel page directory */
    phys = kernel_startup.kernel_phys 
        + (addr_t) kernel_pagedir 
        - (addr_t) scode;
    proc_idle.page_dir_phys = phys;

    /* Set up the last PDE to point to the page directory itself */
    kernel_pagedir[1023] = phys | PRIV_WR | PRIV_KERN | PRIV_PRES;

    /*
     * xxx - Chicken and egg situation here
     * Need to map PHYSMEM, in order to access pool_all.pages, before we can 
     *	  call MemMap() later.
     */
    //entry = PAGE_TABENT(PHYSMEM);
    pt1 = MemAlloc();
    memset(DEMANGLE_PTR(void*, pt1), 0, PAGE_SIZE);
    kernel_pagedir[PAGE_DIRENT(PHYSMEM)] = pt1 | PRIV_WR | PRIV_USER | PRIV_PRES;

    /* Load CR3 */
    __asm__("mov %0, %%cr3" 
        : 
        : "r" (phys));

    /* Enable paging... */
    __asm__("mov %%cr0, %0\n"
        "or %1, %0\n"
        "mov %0, %%cr0" : : "r" (entry), "g" (CR0_PG));

    __asm__("mov %%cr4, %0\n"
        "or %1, %0\n"
        "mov %0, %%cr4" : : "r" (entry), "g" (CR4_PGE));

    /* Reload the flat selectors */
    __asm__("mov %0, %%ds\n"
        "mov %0, %%es\n"
        "mov %1, %%fs\n"
        "mov %0, %%gs\n"
        "mov %0, %%ss\n"
        :
        : "r" (KERNEL_FLAT_DATA), "r" (USER_THREAD_INFO));

    /* Reload CS with the new flat selector */
    __asm__("ljmp %0,$_paging\n"
        ".align 8\n"
        "_paging:"
        :
        : "i" (KERNEL_FLAT_CODE));

    mem_ready = true;

    /* Create a page table for the bottom 4MB */
    /*pt1 = MemAlloc();
    kernel_pagedir[0] = pt1 | PRIV_WR | PRIV_KERN | PRIV_PRES;
    *ADDR_TO_PTE(0xb8000) = 0xb8000 | PRIV_WR | PRIV_PRES;
    __asm__("invlpg %0"
    :
    : "m" (0xb8000));*/

    mem_pages = PHYSICAL(MANGLE_PTR(void*, mem_pages));
    locked_pages = PHYSICAL(MANGLE_PTR(void*, locked_pages));

    /* Map at most 128MB of physical memory */
    MemMap(PHYSMEM, 
        0, 
        PHYSMEM + min(kernel_startup.memory_size, 0x08000000), 
        PRIV_WR | PRIV_PRES | PRIV_KERN);

    kernel_startup.multiboot_info = 
        PHYSICAL(MANGLE_PTR(void*, kernel_startup.multiboot_info));
    kernel_startup.multiboot_info->mods_addr = 
        (addr_t) PHYSICAL(MANGLE_PTR(void*, kernel_startup.multiboot_info->mods_addr));
    kernel_startup.multiboot_info->mmap_addr = 
        (addr_t) PHYSICAL(MANGLE_PTR(void*, kernel_startup.multiboot_info->mmap_addr));

    return true;
}
