/* $Id: memory.c,v 1.5 2002/01/03 15:44:08 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/proc.h>

#include <string.h>

#define MEM_TEMP_START	0xf8000000

extern addr_t kernel_pagedir[];
extern char scode[], ebss[], edata[];
extern process_t proc_idle;

page_pool_t pool_low, pool_all;
addr_t mem_temp_end = MEM_TEMP_START;
uint8_t *locked_pages;

bool mem_ready;

static addr_t MemAllocPool(page_pool_t *pool)
{
	addr_t addr;

	SemAcquire(&pool->sem);
	do
	{
		if (pool->free_pages <= 0)
			return NULL;

		addr = pool->pages[--pool->free_pages];
	} while (addr == (addr_t) -1);
	SemRelease(&pool->sem);

	return addr;
}

static void MemFreePool(page_pool_t *pool, addr_t block)
{
	SemAcquire(&pool->sem);
	pool->pages[pool->free_pages++] = block;
	SemRelease(&pool->sem);
}

static void MemFreePoolRange(page_pool_t *pool, addr_t start, addr_t end)
{
	SemAcquire(&pool->sem);

	start = PAGE_ALIGN(start);
	end = PAGE_ALIGN(end);
	for (; start < end; start += PAGE_SIZE)
		pool->pages[pool->free_pages++] = start;

	SemRelease(&pool->sem);
}

addr_t MemAlloc(void)
{
	return MemAllocPool(&pool_all);
}

void MemFree(addr_t block)
{
	MemFreePool(&pool_all, block);
}

addr_t MemAllocLow(void)
{
	return MemAllocPool(&pool_low);
}

void MemFreeLow(addr_t block)
{
	MemFreePool(&pool_low, block);
}

addr_t MemAllocLowSpan(size_t pages)
{
	unsigned start, i;
	addr_t addr;
	
	SemAcquire(&pool_low.sem);
	for (start = 0; start < pool_low.num_pages; start++)
	{
		for (i = 0; i < pages; i++)
			if (pool_low.pages[start + i] == NULL ||
				(i > 0 && pool_low.pages[start + i] != 
					pool_low.pages[start + i - 1] + PAGE_SIZE))
				break;

		if (i >= pages)
		{
			addr = pool_low.pages[start];

			for (i = 0; i < pages; i++)
				pool_low.pages[start + i] = NULL;
			
			SemRelease(&pool_low.sem);
			return addr;
		}
	}

	SemRelease(&pool_low.sem);
	return NULL;
}

#define PAGETABLE_MAP		(0xffc00000)
#define PAGEDIRECTORY_MAP	(0xffc00000 + (PAGETABLE_MAP / (1024)))
#define ADDR_TO_PDE(v)	(addr_t*)(PAGEDIRECTORY_MAP + \
								(((addr_t) (v) / (1024 * 1024))&(~0x3)))
#define ADDR_TO_PTE(v)	(addr_t*)(PAGETABLE_MAP + ((((addr_t) (v) / 1024))&(~0x3)))

bool MemMap(addr_t virt, addr_t phys, addr_t virt_end, uint8_t priv)
{
	addr_t *pde;
	
	virt = PAGE_ALIGN(virt);
	phys = PAGE_ALIGN(phys);
	virt_end = PAGE_ALIGN(virt_end);
	
	for (; virt < virt_end; virt += PAGE_SIZE, phys += PAGE_SIZE)
	{
		pde = ADDR_TO_PDE(virt);

		/*if (mem_ready && virt < 0x400000)
			wprintf(L"MemMap: mapping %lx => %lx pde = %lx\n",
				virt, phys, *pde);*/

		if (*pde == NULL)
		{
			addr_t pt;

			pt = MemAlloc();

			if (pt == NULL)
				return false;

			*pde = pt | PRIV_WR | PRIV_USER | PRIV_PRES;
		}

		*ADDR_TO_PTE(virt) = phys | priv;
		__asm__("invlpg %0"
			:
			: "m" (virt));
	}

	return true;
}

uint32_t MemTranslate(const void *address)
{
	if (*ADDR_TO_PDE((addr_t) address))
		return *ADDR_TO_PTE((addr_t) address);
	else
		return 0;
}

void *MemMapTemp(const addr_t *phys, unsigned num_pages, uint8_t priv)
{
	void *ptr;

	if (num_pages == 1 &&
		*phys < 0x08000000)
		return PHYSICAL(*phys);

	ptr = (void*) mem_temp_end;
	for (; num_pages > 0; phys++, num_pages--)
	{
		if (!MemMap(mem_temp_end, *phys, mem_temp_end + PAGE_SIZE, priv))
			return NULL;

		mem_temp_end += PAGE_SIZE;
	}

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
	memcpy(pd, kernel_pagedir, PAGE_SIZE);
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
		/*if (phys > kernel_startup.memory_size)
			return false;*/

		assert(phys < kernel_startup.memory_size);
		locked_pages[index] += d;
	}

	return true;
}

bool MemInit(void)
{
	unsigned num_pages;
	addr_t *pages, pt1, pt2, phys;
	uint32_t entry;

	num_pages = kernel_startup.memory_size / PAGE_SIZE;

	/*
	 * Total kernel size composed of:
	 *	Kernel file image
	 *	bss
	 *	Page address stack
	 *	Page lock counts
	 */
	kernel_startup.kernel_data = kernel_startup.kernel_size 
		+ ebss - edata 
		+ num_pages * sizeof(addr_t)
		+ num_pages;
	kernel_startup.kernel_data = PAGE_ALIGN_UP(kernel_startup.kernel_data);

	/* One page of PTEs at one page per PTE = 4MB */
	assert(kernel_startup.kernel_data < PAGE_SIZE * PAGE_SIZE / sizeof(uint32_t));

	/* Page address stack goes after the bss */
	pages = (addr_t*) ebss;
	memset(pages, 0, num_pages * sizeof(addr_t));

	/* Page lock counts go after the stack */
	locked_pages = (uint8_t*) ((addr_t*) ebss + num_pages);
	memset(locked_pages, 0, num_pages);
	
	pool_all.num_pages = num_pages - NUM_LOW_PAGES;
	pool_all.pages = pages + NUM_LOW_PAGES;
	pool_all.free_pages = 0;

	pool_low.num_pages = NUM_LOW_PAGES;
	pool_low.pages = pages;
	pool_low.free_pages = 0;

	/* Memory from BDA=>ROMs */
	MemFreePoolRange(&pool_low, 0x5000, 0xA0000);
	/* Memory from top of ROMs to top of 'low memory' */
	MemFreePoolRange(&pool_low, 0x100000, LOW_MEMORY);

	/* Assume that the ramdisk comes before the kernel */
	assert(kernel_startup.ramdisk_phys < kernel_startup.kernel_phys);

	/* Memory below the ramdisk */
	MemFreePoolRange(&pool_all, LOW_MEMORY, kernel_startup.ramdisk_phys);
	/* Memory between the ramdisk and the kernel */
	MemFreePoolRange(&pool_all, 
		kernel_startup.ramdisk_phys + kernel_startup.ramdisk_size,
		kernel_startup.kernel_phys);
	/* Memory above the kernel */
	MemFreePoolRange(&pool_all, 
		kernel_startup.kernel_phys + kernel_startup.kernel_size, 
		kernel_startup.memory_size);

	/*
	 * Allocate one page table and patch the entries in the page directory:
	 *	- at 0xC0000000 for scode
	 *	- at kernel_phys for identity mapping
	 */
	
	pt1 = MemAlloc();
	kernel_pagedir[PAGE_DIRENT((addr_t) scode)] = pt1 | PRIV_WR | PRIV_USER | PRIV_PRES;
	
	pt2 = MemAlloc();
	kernel_pagedir[PAGE_DIRENT(kernel_startup.kernel_phys)] = pt2 | PRIV_WR | PRIV_USER | PRIV_PRES;
	
	for (phys = 0; 
		phys < kernel_startup.kernel_data; 
		phys += PAGE_SIZE)
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

	/* Load CR3 */
	__asm__("mov %0, %%cr3" 
		: 
		: "r" (phys));

	/* Enable paging... */
	__asm__("mov %cr0, %eax\n"
		"or $0x80000000, %eax\n"
		"mov %eax, %cr0");

	/* Reload the flat selectors */
	__asm__("mov %0, %%ds\n"
		"mov %0, %%es\n"
		"mov %1, %%fs\n"
		"mov %0, %%gs\n"
		"mov %0, %%ss\n"
		:
		: "r" (KERNEL_FLAT_DATA),
			"r" (USER_THREAD_INFO));

	/* Reload CS with the new flat selector */
	__asm__("ljmp %0,$_paging\n"
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

	/* Map at most 128MB of physical memory */
	MemMap(PHYSMEM, 
		0, 
		PHYSMEM + min(kernel_startup.memory_size, 0x08000000), 
		PRIV_WR | PRIV_PRES | PRIV_KERN);

	return true;
}
