#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern byte scode[], edata[], ebss[];
extern dword kernel_pagedir[];
void main();

typedef struct page_pool_t page_pool_t;
struct page_pool_t
{
	//! Stack of addresses, describing each page in the pool
	addr_t* pages;
	//! The number of pages in memory, i.e. memory_top / PAGE_SIZE
	unsigned num_pages;
	//! The number of free pages in the stack
	unsigned free_pages;
};

// pool_low contains all the pages below 1MB
#define NUM_LOW_PAGES	(0x100000 >> PAGE_BITS)
page_pool_t pool_all, pool_low;

//! Marks a range of pages with the specified flags.
/*!
 *	\param	start	Page number of the start of the range
 *	\param	end		Page number of the end of the range
 *	\param	flags	Flags to assign to each of the pages
 */
/*void memMark(int start, int end, dword flags)
{
	//if (flags == PAGE_RESERVED)
		//wprintf(L"Reserved: %08x-%08x\n", start * PAGE_SIZE, end * PAGE_SIZE);

	assert(end < num_pages);

	for (; start < end; start++)
		pages[start].flags = flags;
}*/

//! Allocates a contiguous block of physical memory.
/*!
 *	\param	length	The number of pages to allocate
 *	\return	The physical address of the start of the block. If there was
 *		insufficient free memory, or if a contiguous block could not be found,
 *		returns NULL.
 */
/*addr_t memAlloc(int length)
{
	int i, j;

	for (i = 0; i < num_pages; i++)
	{
		for (j = 0; pages[i + j].flags == PAGE_FREE && j < length; j++)
			;

		if (j == length)
		{
			wprintf(L"allocated %d pages at %x\n", length, i * PAGE_SIZE);
			memMark(i, i + length, PAGE_USED);
			return i * PAGE_SIZE;
		}
	}

	wprintf(L"memAlloc: no pages left!\n");
	halt(0);
	errno = ENOMEM;
	return NULL;
}*/

addr_t memAlloc()
{
	addr_t page;
	assert(pool_all.free_pages > 0);
	pool_all.free_pages--;
	page = pool_all.pages[pool_all.free_pages];
	return page;
}

addr_t memAllocLow()
{
	addr_t page;
	assert(pool_low.free_pages > 0);
	pool_low.free_pages--;
	page = pool_low.pages[pool_low.free_pages];
	assert(page < NUM_LOW_PAGES * PAGE_SIZE);
	return page;
}

//! Frees a block of memory allocated by memAlloc.
/*!
 *	\param	block	The physical address of the start of the block
 *	\param	length	The number of pages occupied by the block
 */
/*void memFree(addr_t block, int length)
{
	if (block)
		memMark(block / PAGE_SIZE, block / PAGE_SIZE + length, PAGE_FREE);
}*/

void memFree(addr_t block)
{
	pool_all.pages[pool_all.free_pages] = block;
	pool_all.free_pages++;
}

void memFreeLow(addr_t block)
{
	assert(block < NUM_LOW_PAGES * PAGE_SIZE);
	pool_low.pages[pool_low.free_pages] = block;
	pool_low.free_pages++;
}

//! Creates a linear-to-physical address mapping in the specified page directory.
/*!
 *	\param	PageDir	The page directory to modify
 *	\param	Virt	The virtual address of the start of the range
 *	\param	Phys	The physical address of the start of the range
 *	\param	pages	The number of pages to map
 *	\param	Privilge	The IA32 privilege flags to assign to each of the 
 *		pages. In addition, PRIV_PRES is assigned to each page.
 *	\return	true if the pages could be mapped, false if there was insufficient
 *		free memory to allocate another page table.
 */
bool memMap(dword *PageDir, dword Virt, dword Phys, dword pages, byte Privilege)
{
	dword PageTabOff, PageDirEnt, Temp;

	/* the top 20 bits of the page table entries are for address translation,
	   the bottom 12 bits (on the 386) are:
	   x x x 0 0 Dirty Accessed 0 0 User Writable Present
	   let the User and Writable bits be set by Privilege */

	Privilege = (Privilege & PRIV_ALL)/* | PRIV_PRES*/;

	//wprintf(L"map %x => %x\n", Virt, Phys);
	for (; pages > 0; pages--, Virt += PAGE_SIZE, Phys += PAGE_SIZE)
	{
		/* get top-level page offset (i.e. page directory entry on x86)
		   from virtual address */
		PageDirEnt = PAGE_DIRENT(Virt);

		if (PageDir[PageDirEnt] == 0)
		/* the page directory entry is zero, which means there is no corresponding
		   page table. Get another 4K of memory and use it to create a new page table. */
		{
			Temp = memAlloc();
			//wprintf(L"%x: new page = %p\n", Virt, Temp);
			if (!Temp)
				return false;

			/* zero the new page table */
			i386_lmemset32(Temp, 0, PAGE_SIZE);

			/* point page directory entry to new page table */

			/* xxx - Privilege of PageDir is not necessarily Privilege of PageTable
			   e.g. call this function while creating the code pages, and the
			   page table ends up read-only (so ALL Ring 3 pages will read-only!!!!) */

			PageDir[PageDirEnt] = Temp | PRIV_PRES | PRIV_WR | PRIV_USER;
		}

		/* get bottom-level page offset (i.e. page table entry on x86)
		   from virtual address */
		Temp = PageDir[PageDirEnt] & -PAGE_SIZE;
		PageTabOff = PAGE_TABENT(Virt);

		// point page table entry to physical address, and set privilege bits
		i386_lpoke32(Temp + PageTabOff * 4, PAGE_NUM(Phys) | Privilege);
	}

	return true;
}

//! Retrieves the linear-to-physical address mapping for the specified address.
/*!
 *	\param	pPageDir	The page directory to query
 *	\param	pAddress	The address to look up
 *	\return	The page table entry associated with the page at pAddress, 
 *		including the physical address and the privilege flags. Returns NULL
 *		if the page has not been mapped.
 */
dword memTranslate(const dword* pPageDir, const void* pAddress)
{
	dword PageDirEnt, PageTable, PageTabOff;

	PageDirEnt = PAGE_DIRENT((dword) pAddress);
	PageTable = (pPageDir[PageDirEnt]) & -PAGE_SIZE;
	if (!PageTable)
		return NULL;

	PageTabOff = PAGE_TABENT((dword) pAddress);
	asm("movl %%gs:(%1),%0" : "=r" (PageTable): "r" (PageTable + PageTabOff * 4));
	return PageTable;
}

#define PAGETABLE_MAP	0xdfc00000
#define ADDR_TO_PDE(v) (addr_t*)(PAGEDIRECTORY_MAP + \
                                (((addr_t)v / (1024 * 1024))&(~0x3)))
#define ADDR_TO_PTE(v) (addr_t*)(PAGETABLE_MAP + ((((addr_t)v / 1024))&(~0x3)))

addr_t* cur_page_dir = (addr_t*) PAGETABLE_MAP;

void memFreeRange(page_pool_t* pool, addr_t start, addr_t end)
{
	int i;

	//start = (start + PAGE_SIZE) & -PAGE_SIZE;
	start &= -PAGE_SIZE;
	end = (end + PAGE_SIZE / 2) & -PAGE_SIZE;
	wprintf(L"memFreeRange: %x to %x", start, end);

	start >>= PAGE_BITS;
	end >>= PAGE_BITS;
	wprintf(L" (%d pages)\n", end - start);
	for (i = 0; i < end - start; i++)
		pool->pages[i + pool->free_pages] = (i + start) * PAGE_SIZE;

	pool->free_pages += end - start;
}

//! Initialises the physical memory manager and creates the initial kernel page
//!		mappings.
/*!
 *	Called by main() during kernel startup.
 *	\return	true if initialisation succeeded, false otherwise. If this function 
 *		returns false then the system will be unstable.
 */
bool INIT_CODE memInit()
{
	addr_t pagedir_phys;
	int num_pages;
	addr_t* pages;
	
	num_pages = _sysinfo.memory_top / PAGE_SIZE;
	_sysinfo.kernel_data = _sysinfo.kernel_size + ebss - edata + num_pages * sizeof(addr_t);
	pages = (addr_t*) ebss;
	i386_lmemset32((addr_t) pages, 0, num_pages * sizeof(addr_t));
	
	pool_all.num_pages = num_pages - NUM_LOW_PAGES;
	pool_all.pages = pages + NUM_LOW_PAGES;
	pool_all.free_pages = 0;

	pool_low.num_pages = NUM_LOW_PAGES;
	pool_low.pages = pages;
	pool_low.free_pages = 0;

	memFreeRange(&pool_low, 0x5000, 0xA0000);

	memFreeRange(&pool_all, 
		0x100000, _sysinfo.kernel_phys);
	memFreeRange(&pool_all, 
		_sysinfo.kernel_phys + _sysinfo.kernel_data, _sysinfo.ramdisk_phys);
	memFreeRange(&pool_all, 
		_sysinfo.ramdisk_phys + _sysinfo.ramdisk_size, _sysinfo.memory_top);

	/*memMark(0, 5, PAGE_RESERVED);

	memMark(0xA0, 0x100, PAGE_RESERVED);
	
	memMark(_sysinfo.ramdisk_phys / PAGE_SIZE,
		(_sysinfo.ramdisk_phys + _sysinfo.ramdisk_size) / PAGE_SIZE, PAGE_RESERVED);
	memMark(_sysinfo.kernel_phys / PAGE_SIZE,
		(_sysinfo.kernel_phys + _sysinfo.kernel_data) / PAGE_SIZE, PAGE_RESERVED);*/
	
	memMap(kernel_pagedir, PAGE_SIZE, PAGE_SIZE, 
		_sysinfo.memory_top / PAGE_SIZE - 1, PRIV_KERN | PRIV_WR | PRIV_PRES);
	memMap(kernel_pagedir, 0xa0000, 0xa0000, 16, PRIV_USER | PRIV_WR | PRIV_PRES);
	memMap(kernel_pagedir, 0xb8000, 0xb8000, 1, PRIV_USER | PRIV_WR | PRIV_PRES);
	memMap(kernel_pagedir, (addr_t) scode, _sysinfo.kernel_phys, 
		_sysinfo.kernel_data / PAGE_SIZE, PRIV_KERN | PRIV_WR | PRIV_PRES);
	//memMap(kernel_pagedir, 0xff000000, 0xff000000, 1048576 / 4096, PRIV_USER | PRIV_WR);

	pagedir_phys = (addr_t) kernel_pagedir - (addr_t) scode + _sysinfo.kernel_phys;
	//kernel_pagedir[PAGE_DIRENT((addr_t) cur_page_dir)] = pagedir_phys | PRIV_KERN | PRIV_WR | PRIV_PRES;
	asm("mov	%0, %%cr3" : : "r" (pagedir_phys));
	
	//wprintf(L"main (%p) maps to %x\n", main, memTranslate(kernel_pagedir, main));
	//wprintf(L"_con_x (%p) maps to %x\n", &_con_x, memTranslate(kernel_pagedir, &_con_x));
	//wprintf(L"text video buffer maps to %x\n", memTranslate(kernel_pagedir, (void*) 0xb8000));
	//wprintf(L"cur_page_dir = %x\n", kernel_pagedir[PAGE_DIRENT((addr_t) cur_page_dir)]);
	//wprintf(L"addr 0x100000 = %p = %x\n", 
		//ADDR_TO_PTE(0x10000), *ADDR_TO_PTE(0x10000));

	asm("mov	$0x20,%%eax ;"
		"mov	%%eax,%%ds ;"
		"mov	%%eax,%%es ;"
		"mov	%%eax,%%fs ;"
		"mov	%%eax,%%gs ;"
		"mov	%%eax,%%ss ;" : : : "eax");
	asm("mov	%%cr0,%%eax ;"
		"or		$0x80000000,%%eax ;"
		"mov	%%eax,%%cr0" : : : "eax");
	asm("ljmp	$0x18,$_paging ;"
		"_paging:");

	return true;
}

