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

bool	MemInit(void);
addr_t	MemAlloc(void);
void	MemFree(addr_t block);
addr_t	MemAllocLow(void);
void	MemFreeLow(addr_t block);
addr_t	MemAllocLowSpan(size_t pages);
bool	MemMap(addr_t virt, addr_t phys, addr_t virt_end, uint8_t priv);
uint32_t	MemTranslate(const void *address);
void *	MemMapTemp(addr_t phys, size_t bytes, uint8_t priv);
void	MemUnmapTemp(void);
addr_t	MemAllocPageDir(void);
bool	MemVerifyBuffer(const void *buf, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif