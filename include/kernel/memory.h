/* $Id: memory.h,v 1.3 2002/01/02 21:15:22 pavlovskii Exp $ */
#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <kernel/arch.h>
#include <kernel/kernel.h>

typedef struct page_pool_t page_pool_t;
struct page_pool_t
{
	semaphore_t sem;
	/*! Stack of addresses, describing each page in the pool */
	addr_t *pages;
	/*! The number of pages in memory, i.e. memory_top / PAGE_SIZE */
	unsigned num_pages;
	/*! The number of free pages in the stack */
	unsigned free_pages;
};

extern page_pool_t pool_all, pool_low;

#define LOW_MEMORY		0x100000
/* pool_low contains all the pages below 1MB */
#define NUM_LOW_PAGES	(LOW_MEMORY >> PAGE_BITS)

#define PAGE_ALIGN(addr)	((addr) & -PAGE_SIZE)
#define PAGE_ALIGN_UP(addr)	(((addr) + PAGE_SIZE - 1) & -PAGE_SIZE)

/** Initializes the physical memory manager */
bool	MemInit(void);
/** Allocates one physical page */
addr_t	MemAlloc(void);
/** Frees one physical page */
void	MemFree(addr_t block);
/** Allocates one physical page from the low-memory pool */
addr_t	MemAllocLow(void);
/** Frees one low-memory physical page */
void	MemFreeLow(addr_t block);
/** Allocates a contiguous span of low-memory pages */
addr_t	MemAllocLowSpan(size_t pages);
/** Creates a virtual-to-physical mapping in the current address space */
bool	MemMap(addr_t virt, addr_t phys, addr_t virt_end, uint8_t priv);
/** Returns the physical address associated with a virtual page */
uint32_t MemTranslate(const void *address);
/** Creates a temporary physical-to-virtual mapping */
void *	MemMapTemp(const addr_t *phys, unsigned num_pages, uint8_t priv);
/** Unmaps all temporary physical-to-virtual mappings */
void	MemUnmapTemp(void);
/** Allocates a page directory */
addr_t	MemAllocPageDir(void);
/** Verifies that the whole of a virtual buffer is accessible */
bool	MemVerifyBuffer(const void *buf, size_t bytes);
/** Locks or unlocks a region of physical memory */
bool	MemLockPages(addr_t phys, unsigned pages, bool do_lock);

#ifdef __cplusplus
}
#endif

#endif