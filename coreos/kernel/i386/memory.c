/*
 * $History: memory.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 8/03/04    Time: 20:34
 * Updated in $/coreos/kernel/i386
 * Moved bitmap functions into their own file
 */

#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/init.h>
#include <kernel/multiboot.h>
#include <kernel/counters.h>
#include <kernel/bitmap.h>

#include <stdio.h>
#include <string.h>

#define NUM_TEMP_MAPPINGS    65535
#define TEMP_MAPPING_INDEX_TO_VIRT(index) \
    ((void*) ((uint8_t*) ADDR_TEMP_VIRT + (index) * PAGE_SIZE))

extern addr_t kernel_pagedir[];
extern char scode[], ebss[], edata[], _data_start__[];
extern process_t proc_idle;

page_phys_t *mem_pages;
addr_t mem_pages_phys;
unsigned mem_num_pages, mem_num_pages_current;
pfn_list_t mem_free =        { { 0 }, PFN_NULL, PFN_NULL, 0 };
pfn_list_t mem_free_low =    { { 0 }, PFN_NULL, PFN_NULL, 0 };
pfn_list_t mem_zero =        { { 0 }, PFN_NULL, PFN_NULL, 0 };
uint8_t mem_temp_in_use[(NUM_TEMP_MAPPINGS + 7) / 8];

int mem_allocs, mem_frees, mem_zeroes, mem_temp_hits, mem_temp_misses, 
    mem_pamap_quick, mem_pamap_slow;

static spinlock_t spin_pages;

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


#define PFN_IS_VALID(pfn)    ((pfn) < mem_num_pages_current)
#define ADDR_IS_VALID(addr)    \
    ((addr) < mem_pages_phys || \
    (addr) >= mem_pages_phys + sizeof(*mem_pages) * mem_num_pages)


#if 0
static void MemValidateList(pfn_list_t *list)
{
    pfn_t pfn, prev;

    SpinAcquire(&list->spin);

    prev = PFN_NULL;
    pfn = list->first;
    while (pfn != PFN_NULL)
    {
        if (!PFN_IS_VALID(pfn))
        {
            wprintf(L"MemValidateList(%p): pfn %u failed validation\n",
                list, pfn);
            assert(PFN_IS_VALID(pfn));
        }

        if (!ADDR_IS_VALID(PHYS_TO_PFN(pfn)))
        {
            wprintf(L"MemValidateList(%p): address %08x failed validation\n",
                list, PFN_TO_PHYS(pfn));
            assert(ADDR_IS_VALID(PFN_TO_PHYS(pfn)));
        }

        assert(mem_pages[pfn].u.list.prev == prev);

        prev = pfn;
        pfn = mem_pages[pfn].u.list.next;
    }

    SpinRelease(&list->spin);
}
#else
void MemValidateList(pfn_list_t *list)
{
    SpinAcquire(&list->spin);

    if (list->first != PFN_NULL)
    {
        if (!PFN_IS_VALID(list->first))
        {
            wprintf(L"MemValidateList(%p): first pfn %u failed validation\n",
                list, list->first);
            assert(PFN_IS_VALID(list->first));
        }

        if (!ADDR_IS_VALID(PHYS_TO_PFN(list->first)))
        {
            wprintf(L"MemValidateList(%p): first address %08x failed validation\n",
                list, PFN_TO_PHYS(list->first));
            assert(ADDR_IS_VALID(PFN_TO_PHYS(list->first)));
        }
    }

    if (list->last != PFN_NULL)
    {
        if (!PFN_IS_VALID(list->last))
        {
            wprintf(L"MemValidateList(%p): last pfn %u failed validation\n",
                list, list->last);
            assert(PFN_IS_VALID(list->last));
        }

        if (!ADDR_IS_VALID(PHYS_TO_PFN(list->last)))
        {
            wprintf(L"MemValidateList(%p): last address %08x failed validation\n",
                list, PFN_TO_PHYS(list->last));
            assert(ADDR_IS_VALID(PFN_TO_PHYS(list->last)));
        }
    }

    SpinRelease(&list->spin);
}
#endif


page_phys_t *MemLookupAllocatedPage(struct vm_desc_t *desc, addr_t offset)
{
    return NULL;
}


static void MemAddToList(pfn_list_t *list, addr_t addr, uint16_t flags)
{
    page_phys_t *page;
    pfn_t pfn;

    pfn = PHYS_TO_PFN(addr);
    assert(addr % PAGE_SIZE == 0);
    assert(PFN_IS_VALID(pfn));
    assert(ADDR_IS_VALID(addr));

    SpinAcquire(&list->spin);
    page = mem_pages + pfn;
    page->flags = (page->flags & ~PAGE_STATE_MASK) | (flags & PAGE_STATE_MASK);

    if (list->last != PFN_NULL)
    {
        assert(PFN_IS_VALID(list->last));
        mem_pages[list->last].u.list.next = pfn;
    }

    page->u.list.prev = list->last;
    page->u.list.next = PFN_NULL;
    list->last = pfn;
    if (list->first == PFN_NULL)
        list->first = pfn;
    list->num_pages++;

    SpinRelease(&list->spin);

    //wprintf(L"MemAddToList(%p, %08x)\n", list, addr);
    MemValidateList(list);
}


static bool MemRemoveHead(pfn_list_t *list, addr_t *addr)
{
    page_phys_t *page;
    pfn_t pfn;

    SpinAcquire(&list->spin);
    if (list->first == PFN_NULL)
    {
        SpinRelease(&list->spin);
        *addr = NULL;
        return false;
    }

    pfn = list->first;
    assert(PFN_IS_VALID(pfn));
    page = mem_pages + pfn;

    if (page->u.list.next != PFN_NULL)
    {
        assert(PFN_IS_VALID(page->u.list.next));
        mem_pages[page->u.list.next].u.list.prev = page->u.list.prev;
    }

    if (page->u.list.prev != PFN_NULL)
    {
        assert(PFN_IS_VALID(page->u.list.prev));
        mem_pages[page->u.list.prev].u.list.next = page->u.list.next;
    }

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
    *addr = PFN_TO_PHYS(pfn);

    //wprintf(L"MemRemoveHead(%p, %08x)\n", list, *addr);
    MemValidateList(list);
    return true;
}


addr_t MemAllocFromList(pfn_list_t *list)
{
    addr_t ret;
    if (MemRemoveHead(list, &ret))
    {
        assert(ADDR_IS_VALID(ret));
        assert(ret != NULL);
        return ret;
    }
    else
        return NULL;
}


addr_t MemAlloc(void)
{
    addr_t ret;
    ret = MemAllocFromList(&mem_zero);
    if (ret == NULL)
    {
        ret = MemAllocFromList(&mem_free);

        if (ret != NULL)
        {
            void *virt;
            virt = MemMapTemp(ret);
            memset(virt, 0, PAGE_SIZE);
            MemUnmapTemp(ret);
        }
    }

    if (ret != NULL)
        KeAtomicInc(&mem_allocs);
    return ret;
}


void MemFree(addr_t block)
{
    KeAtomicInc(&mem_frees);
    assert(block >= ADDR_LOW_END_PHYS);
    assert(ADDR_IS_VALID(block));
    MemAddToList(&mem_free, block, PAGE_STATE_FREE);
}


addr_t MemAllocLow(void)
{
    return MemAllocFromList(&mem_free_low);
}


void MemFreeLow(addr_t block)
{
    assert(block < ADDR_LOW_END_PHYS);
    assert(ADDR_IS_VALID(block));
    MemAddToList(&mem_free_low, block, PAGE_STATE_FREE | PAGE_FLAG_LOW);
}


bool MemMapRange(const void *virt, addr_t phys, const void *virt_end, uint16_t priv)
{
    addr_t *pde, virt_addr, virt_end_addr;

    virt_addr = PAGE_ALIGN((addr_t) virt);
    phys = PAGE_ALIGN(phys);
    virt_end_addr = PAGE_ALIGN((addr_t) virt_end);

    for (; virt_addr < virt_end_addr; virt_addr += PAGE_SIZE, phys += PAGE_SIZE)
    {
        pde = ADDR_TO_PDE(virt_addr);

        if (*pde == 0)
        {
            addr_t pt;

            pt = MemAllocLow();
            if (pt == NULL)
            {
                assert(false && "Out of memory in MemMapRange");
                return false;
            }

            memset(PHYSICAL(pt), 0, PAGE_SIZE);
            if (virt_addr >= 0x80000000)
            {
                pt |= PAGE_GLOBAL;
                kernel_pagedir[PAGE_DIRENT(virt_addr)] = pt 
                    | PRIV_WR | PRIV_USER | PRIV_PRES;
            }

            *pde = pt | PRIV_WR | PRIV_USER | PRIV_PRES;
        }

        *ADDR_TO_PTE(virt_addr) = phys | priv;
        __asm__("invlpg %0"
            :
            : "m" (*(char*) virt_addr));
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

        pt = MemAllocLow();
        if (pt == NULL)
        {
            assert(false && "Out of memory in MemSetPageState");
            return false;
        }

        memset(PHYSICAL(pt), 0, PAGE_SIZE);
        if (v >= 0x80000000)
        {
            pt |= PAGE_GLOBAL;
            kernel_pagedir[PAGE_DIRENT(v)] = pt 
                | PRIV_WR | PRIV_USER | PRIV_PRES;
        }

        *pde = pt | PRIV_WR | PRIV_USER | PRIV_PRES;
    }

    *ADDR_TO_PTE(v) = (*ADDR_TO_PTE(v) & ~PAGE_STATEMASK) | state;
    __asm__("invlpg %0"
        :
        : "m" (*(char*) virt));

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


static unsigned MemAllocateTempMapping(addr_t phys)
{
    uint8_t *virt;
    unsigned index;
    uint32_t addr;
    pfn_t pfn_this, pfn_other;

    pfn_this = PHYS_TO_PFN(phys);

    if ((mem_pages[pfn_this].flags & PAGE_FLAG_TEMP_IN_USE) == 0)
        index = pfn_this % NUM_TEMP_MAPPINGS;
    else
        index = mem_pages[pfn_this].temp_index;

    while (true)
    {
        virt = TEMP_MAPPING_INDEX_TO_VIRT(index);
        addr = MemTranslate(virt);
        pfn_other = PHYS_TO_PFN(addr);
        if ((addr & PRIV_PRES) == 0 ||
            pfn_other == pfn_this)
            return index;
        else
        {
            if (mem_pages[pfn_other].flags & PAGE_FLAG_TEMP_IN_USE)
            {
                index++;
                if (index >= NUM_TEMP_MAPPINGS)
                    index = 0;
            }
            else
                return index;
        }
    }
}


static void MemAssignTempMapping(unsigned index, addr_t phys)
{
    pfn_t pfn;
    uint8_t *virt;

    pfn = PHYS_TO_PFN(phys);
    virt = TEMP_MAPPING_INDEX_TO_VIRT(index);
    if ((MemTranslate(virt) & -PAGE_SIZE) == phys)
        KeAtomicInc(&mem_temp_hits);
    else
    {
        KeAtomicInc(&mem_temp_misses);
        MemMapRange(virt, 
            phys, 
            virt + PAGE_SIZE, 
            PRIV_PRES | PRIV_WR | PRIV_RD | PRIV_KERN);
    }

    mem_pages[pfn].temp_index = index;
    mem_pages[pfn].flags |= PAGE_FLAG_TEMP_IN_USE;
    BitmapSetBit(mem_temp_in_use, index, true);
}


void *MemMapTemp(addr_t phys)
{
    unsigned index;
    SpinAcquire(&spin_pages);
    index = MemAllocateTempMapping(phys);
    MemAssignTempMapping(index, phys);
    SpinRelease(&spin_pages);
    return TEMP_MAPPING_INDEX_TO_VIRT(index);
}


void MemUnmapTemp(addr_t addr)
{
    pfn_t pfn;

    SpinAcquire(&spin_pages);
    pfn = PHYS_TO_PFN(addr);
    assert(mem_pages[pfn].flags & PAGE_FLAG_TEMP_IN_USE);
    if ((mem_pages[pfn].flags & PAGE_FLAG_TEMP_LOCKED) == 0)
    {
        mem_pages[pfn].flags &= ~PAGE_FLAG_TEMP_IN_USE;
        BitmapSetBit(mem_temp_in_use, mem_pages[pfn].temp_index, false);
    }

    SpinRelease(&spin_pages);
}


addr_t MemAllocPageDir(void)
{
    addr_t page_dir, *pd;

    page_dir = MemAlloc();
    pd = MemMapTemp(page_dir);
    memset(pd, 0, 512 * sizeof(*pd));
    memcpy(pd + 512, kernel_pagedir + 512, 511 * sizeof(*pd));
    pd[1023] = page_dir | PRIV_KERN | PRIV_WR | PRIV_PRES;
    MemUnmapTemp(page_dir);

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
    pfn_t pfn;
    int d;

    phys = PAGE_ALIGN(phys);
    pfn = PHYS_TO_PFN(phys);
    d = do_lock ? 1 : -1;

    //wprintf(L"MemLockPages(%08x, %8u) from %p\n",
        //phys, pages, __builtin_return_address(0));
    for (; pages > 0; pages--, pfn++, phys += PAGE_SIZE)
    {
        /* xxx - drivers could try to lock memory outside of RAM (e.g. the 
         *  video frame buffer).
         * The kernel isn't going to do anything with these pages anyway, so
         *  just ignore attempts to lock/unlock them. */
        if (phys < kernel_startup.memory_size)
        {
            if (pfn >= mem_num_pages_current)
            {
                wprintf(L"MemLockPages: pfn %u beyond mem_num_pages_current %u\n",
                    pfn, mem_num_pages_current);
                assert(pfn < mem_num_pages_current);
            }

            mem_pages[pfn].locks += d;
        }
    }

    return true;
}


uint8_t MemIsPageLocked(addr_t phys)
{
    if (phys >= kernel_startup.memory_size)
        return (uint8_t) -1;
    else
        return mem_pages[PHYS_TO_PFN(PAGE_ALIGN_UP(phys))].locks;
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
        {
            if (*ptr != NULL)
                MemLockPages(*ptr, 1, false);
        }

        free(array);
    }
    else
        KeAtomicDec(&array->refs);
}


void *MemMapPageArray(page_array_t *array)
{
    int index;
    unsigned i;
    uint8_t *ret;

    if (array->num_pages == 1)
    {
        KeAtomicInc(&mem_pamap_quick);
        return (uint8_t*) MemMapTemp(array->pages[0]) + array->mod_first_page;
    }

    SpinAcquire(&spin_pages);
    index = BitmapFindClearRange(mem_temp_in_use + _countof(mem_temp_in_use) / 2, 
        NUM_TEMP_MAPPINGS / 2, 
        array->num_pages);
    if (index == -1)
    {
        index = BitmapFindClearRange(mem_temp_in_use, 
            NUM_TEMP_MAPPINGS, 
            array->num_pages);
        if (index == -1)
        {
            SpinRelease(&spin_pages);
            return NULL;
        }
    }

    ret = TEMP_MAPPING_INDEX_TO_VIRT(index);

    for (i = 0; i < array->num_pages; i++)
    {
        BitmapSetBit(mem_temp_in_use,
            index + i,
            true);
        MemAssignTempMapping(index + i, array->pages[i]);
        assert((MemTranslate(ret + i * PAGE_SIZE) & -PAGE_SIZE) == array->pages[i]);
    }

    SpinRelease(&spin_pages);
    KeAtomicInc(&mem_pamap_slow);
    return ret + array->mod_first_page;
}


void MemUnmapPageArray(page_array_t *array)
{
    unsigned i;

    for (i = 0; i < array->num_pages; i++)
        MemUnmapTemp(array->pages[i]);
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

        ptr = MemMapTemp(addr);
        //wprintf(L"%08x => %p\n", addr, ptr);
        memset(ptr, 0, PAGE_SIZE);
        MemUnmapTemp(addr);
        MemAddToList(&mem_zero, addr, PAGE_STATE_ZERO);
        KeAtomicInc(&mem_zeroes);
    }
}


#if 0
static void memset_phys(addr_t phys, int value, size_t bytes)
{
    addr_t addr, align_start, end, align_end;
    void *virt;

    end = phys + bytes;
    align_start = PAGE_ALIGN_UP(phys);
    if (align_start > end)
        align_start = end;
    align_end = PAGE_ALIGN(end);

    /* Map first part page */
    if (phys < align_start)
    {
        addr = PAGE_ALIGN(phys);
        virt = MemMapTemp(addr);
        memset((uint8_t*) virt + phys - addr, value, align_start - phys);
        MemUnmapTemp(addr);
    }

    /* Map middle pages */
    if (align_start < end)
    {
        addr = align_start;
        while (addr < align_end)
        {
            virt = MemMapTemp(addr);
            memset(virt, value, PAGE_SIZE);
            MemUnmapTemp(addr);
            addr += PAGE_SIZE;
        }
    }

    /* Map end */
    if (align_start <= align_end &&
        align_end < end)
    {
        addr = align_end;
        virt = MemMapTemp(addr);
        memset(virt, value, end - align_end);
        MemUnmapTemp(addr);
    }
}
#endif


/*!
 * \brief Initializes the physical memory manager
 *
 *    This is the first function called by \p KernelMain. Its purpose is to get
 *    the CPU into an environment suitable for running the rest of the kernel,
 *    and performs the following tasks:
 *
 *    - Sets aside physical page pools for main and low memory
 *    - Distinguishes between usable and unusable memory
 *
 *    \return    \p true
 */
bool MemInit(void)
{
    multiboot_module_t *mbmods;
    unsigned i, num_bottom_pages;

    SpinInit(&spin_pages);

    mem_num_pages = kernel_startup.memory_size / PAGE_SIZE;
    kernel_startup.kernel_data = PAGE_ALIGN_UP(kernel_startup.kernel_size + ebss - edata);

    mbmods = PHYSICAL(kernel_startup.multiboot_info->mods_addr);
    mem_pages_phys = mbmods[kernel_startup.multiboot_info->mods_count - 1].mod_end;
    mem_pages_phys = PAGE_ALIGN_UP(mem_pages_phys);
    mem_pages = PHYSICAL(mem_pages_phys);

    //num_bottom_pages = min(mem_num_pages, PHYS_TO_PFN(0x00400000));
    //num_bottom_pages = mem_num_pages;
    //if (mem_pages_phys + num_bottom_pages * sizeof(*mem_pages) > 0x00400000)
        //num_bottom_pages = (0x00400000 - mem_pages_phys) / sizeof(*mem_pages);
    //mem_num_pages = num_bottom_pages;
    //kernel_startup.memory_size = mem_num_pages * PAGE_SIZE;

    num_bottom_pages = mem_num_pages;

    wprintf(L"bottom = %u, low = %u, num = %u, mem_pages = %08x..%08x\n",
        num_bottom_pages, 
        PHYS_TO_PFN(ADDR_LOW_END_PHYS), 
        mem_num_pages, 
        mem_pages_phys, 
        mem_pages_phys + mem_num_pages * sizeof(*mem_pages));

    mem_num_pages_current = num_bottom_pages;
    memset(mem_pages, 0, num_bottom_pages * sizeof(*mem_pages));

    MemLockPages(0,
        1,
        true);
    MemLockPages(0x000A0000,
        (0x00100000 - 0x000A0000) / PAGE_SIZE,
        true);
    MemLockPages(kernel_startup.kernel_phys, 
        PAGE_ALIGN_UP(kernel_startup.kernel_data) / PAGE_SIZE, 
        true);

    MemLockPages(mem_pages_phys, 
        //(PFN_TO_PHYS(num_bottom_pages) - mem_pages_phys) / PAGE_SIZE,
        PAGE_ALIGN_UP(num_bottom_pages * sizeof(*mem_pages)) / PAGE_SIZE,
        true);
    MemLockPages(PAGE_ALIGN(kernel_startup.multiboot_info->mods_addr & 0x0fffffff),
        ADDRESS_AND_SIZE_TO_SPAN_PAGES(kernel_startup.multiboot_info->mods_addr & 0x0fffffff, 
            sizeof(*mbmods) * kernel_startup.multiboot_info->mods_count),
        true);
    for (i = 0; i < kernel_startup.multiboot_info->mods_count; i++)
    {
        MemLockPages(PAGE_ALIGN(mbmods[i].string),
            1,
            true);
        MemLockPages(PAGE_ALIGN(mbmods[i].mod_start), 
            ADDRESS_AND_SIZE_TO_SPAN_PAGES(mbmods[i].mod_start, mbmods[i].mod_end - mbmods[i].mod_start),
            true);
    }

    for (i = 0; i < PHYS_TO_PFN(ADDR_LOW_END_PHYS); i++)
        if (mem_pages[i].locks == 0)
            MemAddToList(&mem_free_low, PFN_TO_PHYS(i), PAGE_STATE_FREE | PAGE_FLAG_LOW);
        else
            mem_pages[i].flags |= PAGE_STATE_RESERVED;

    for (i = PHYS_TO_PFN(ADDR_LOW_END_PHYS); i < num_bottom_pages; i++)
    {
        if (mem_pages[i].locks == 0)
            MemAddToList(&mem_free, PFN_TO_PHYS(i), PAGE_STATE_FREE);
        else
            mem_pages[i].flags |= PAGE_STATE_RESERVED;
    }

    //wprintf(L"MemInit: mapping all of pages array...");
    MemMapRange((page_phys_t*) ADDR_PAGES_VIRT,
        mem_pages_phys,
        (page_phys_t*) ADDR_PAGES_VIRT + mem_num_pages,
        PRIV_RD | PRIV_WR | PRIV_PRES | PRIV_KERN);
    mem_pages = (page_phys_t*) ADDR_PAGES_VIRT;
    memset(mem_pages + num_bottom_pages,
        0,
        (mem_num_pages - num_bottom_pages) * sizeof(*mem_pages));
    mem_num_pages_current = mem_num_pages;
    //wprintf(L"done\n");

    MemLockPages(mem_pages_phys, 
        (PAGE_ALIGN_UP(mem_num_pages) * sizeof(*mem_pages)) / PAGE_SIZE,
        true);

    for (i = num_bottom_pages; i < mem_num_pages; i++)
        if (mem_pages[i].locks == 0)
            MemAddToList(&mem_free, PFN_TO_PHYS(i), PAGE_STATE_FREE);
        else
            mem_pages[i].flags |= PAGE_STATE_RESERVED;

    for (i = 0; i < PHYS_TO_PFN(0x00400000); i++)
    {
        mem_pages[i].flags = (mem_pages[i].flags & PAGE_STATE_MASK) | 
            PAGE_FLAG_TEMP_IN_USE | PAGE_FLAG_TEMP_LOCKED;
        mem_pages[i].temp_index = i;
        BitmapSetBit(mem_temp_in_use, i, true);
        MemMapRange(TEMP_MAPPING_INDEX_TO_VIRT(i),
            PFN_TO_PHYS(i),
            TEMP_MAPPING_INDEX_TO_VIRT(i + 1),
            PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);
    }

    //wprintf(L"MemInit: validating free list\n");
    MemValidateList(&mem_free);
    //wprintf(L"MemInit: validating free low list\n");
    MemValidateList(&mem_free_low);
    //wprintf(L"MemInit: validating zero list\n");
    MemValidateList(&mem_zero);

    return true;
}
