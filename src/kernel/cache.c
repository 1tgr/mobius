/* $Id: cache.c,v 1.7 2002/01/15 00:12:58 pavlovskii Exp $ */

/* xxx - what if block_size > PAGE_SIZE? */

#include <kernel/kernel.h>
#include <kernel/cache.h>
#include <kernel/driver.h>
#include <kernel/memory.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#define ALIGN(num, to)		((num) & -(to))
#define ALIGN_UP(num, to)	(((num) + (to) - 1) & -(to))

typedef struct cache_block_t cache_block_t;
struct cache_block_t
{
	bool is_valid;
};

/*!
 *	\brief	Creates a file cache object
 *
 *	\param	block_size	The size of one cache block. This is usually the 
 *		file system's cluster size.
 *	\return	A pointer to a file cache object
 */
cache_t *CcCreateFileCache(size_t block_size)
{
	cache_t *cc;
	unsigned temp;

	cc = malloc(sizeof(cache_t));
	SemInit(&cc->lock);
	cc->block_size = block_size;

	temp = block_size;
	for (cc->block_shift = 0; (temp & 1) == 0; cc->block_shift++)
		temp >>= 1;

	cc->num_blocks = 0;
	cc->blocks = NULL;
	cc->num_pages = 0;
	cc->pages = NULL;
	wprintf(L"CcCreateFileCache(%p): block_size = %u block_shift = %u\n", 
		cc, cc->block_size, cc->block_shift);
	return cc;
}

/*!
 *	\brief	Deletes a file cache object
 *
 *	Any cached pages are flushed and freed.
 *	\param	cc	A pointer to the cache object
 */
void CcDeleteFileCache(cache_t *cc)
{
	unsigned i;

	wprintf(L"CcDeleteFileCache(%p): %u blocks, %u pages\n", 
		cc, cc->num_blocks, cc->num_pages);
	SemAcquire(&cc->lock);
	for (i = 0; i < cc->num_pages; i++)
		if (cc->pages[i] != -1)
			MemFree(cc->pages[i]);

	free(cc->blocks);
	free(cc->pages);
	SemRelease(&cc->lock);
	free(cc);
}

/*!
 *	\brief	Locks a cache block in memory, allocating it if necessary
 *
 *	The block will only contain valid data if \p CcIsBlockValid returned
 *	\p true before this function was called. This function does not
 *	change the validity of the block.
 *
 *	Because blocks are reference counted, \p CcReleaseBlock must be called 
 *	when you have finished with the block.
 *
 *	\param	cc	A pointer to the file cache object
 *	\param	offset	The offset of the block in the file
 *	\return	A pointer to the start of the block in memory
 */
void *CcRequestBlock(cache_t *cc, uint64_t offset)
{
	unsigned block, page, old, i;
	uint8_t *ptr;

	SemAcquire(&cc->lock);
	offset = ALIGN(offset, cc->block_size);
	block = offset >> cc->block_shift;
	page = (block * cc->block_size) / PAGE_SIZE;
	wprintf(L"CcRequestBlock(%p): offset = %u block = %u/%u page = %u/%u\n",
		cc, (uint32_t) offset, 
		block, cc->num_blocks,
		page, cc->num_pages);
	if (block >= cc->num_blocks)
	{
		old = cc->num_blocks;
		cc->num_blocks = block + 1;
		cc->blocks = realloc(cc->blocks, sizeof(cache_block_t) * cc->num_blocks);
		assert(cc->blocks != NULL);
		for (i = old; i < cc->num_blocks; i++)
			cc->blocks[block].is_valid = false;
		
		old = cc->num_pages;
		cc->num_pages = PAGE_ALIGN_UP(cc->num_blocks * cc->block_size) / PAGE_SIZE;
		if (cc->num_pages > old)
		{
			cc->pages = realloc(cc->pages, sizeof(addr_t) * cc->num_pages);
			assert(cc->pages != NULL);
			for (i = old; i < cc->num_pages; i++)
				cc->pages[i] = -1;
		}
		
		if (cc->pages[page] == -1)
		{
			cc->pages[page] = MemAlloc();
			wprintf(L"CcRequestBlock: new page: phys = %x\n", cc->pages[page]);
		}
	}
	
	assert(page < cc->num_pages);
	assert(cc->pages[page] != -1);
	MemLockPages(cc->pages[page], 1, true);
	SemRelease(&cc->lock);
	ptr = MemMapTemp(&cc->pages[page], 1, 
		PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);
	if (ptr == NULL)
		return NULL;
	else
	{
		wprintf(L"CcRequestBlock: ptr = %p mod = %u\n",
			ptr, (block << cc->block_shift) % PAGE_SIZE);
		return ptr + (block << cc->block_shift) % PAGE_SIZE;
	}
}

/*!
 *	\brief	Returns \p true if the specified cache block is valid
 *
 *	A block is invalid if any of the following are true:
 *	- Its offset is beyond the extent of the cached data
 *	- Its memory has been paged out
 *	- It has never been made valid
 *
 *	To change the validity of a block, call \p CcRequestBlock followed by
 *	\p CcReleaseBlock.
 *
 *	\param	cc	A pointer to the file cache object
 *	\param	offset	The offset of the block in the file
 *	\return	\p true if the block is valid
 */
bool CcIsBlockValid(cache_t *cc, uint64_t offset)
{
	unsigned block;

	block = ALIGN(offset, cc->block_size) >> cc->block_shift;
	if (block >= cc->num_blocks)
		return false;
	else
		return cc->blocks[block].is_valid;
}

/*!
 *	\brief	Releases a cache block
 *
 *	Any memory associated with the block is made available for paging, and
 *	the validity of the block is changed as appropriate.
 *
 *	\param	cc	A pointer to the file cache object
 *	\param	offset	The offset of the block in the file
 *	\param	isValid	Indicates whether the block's memory now contains valid
 *		data.
 */
void CcReleaseBlock(cache_t *cc, uint64_t offset, bool isValid)
{
	unsigned block, page;

	offset = ALIGN(offset, cc->block_size);
	block = offset >> cc->block_shift;
	assert(block < cc->num_blocks);

	page = (block * cc->block_size) / PAGE_SIZE;
	SemAcquire(&cc->lock);
	cc->blocks[block].is_valid = isValid;

	if (cc->pages[page] != -1)
		MemLockPages(cc->pages[page], 1, false);

	MemUnmapTemp();
	SemRelease(&cc->lock);
}
