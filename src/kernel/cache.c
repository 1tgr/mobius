/* $Id: cache.c,v 1.4 2002/01/06 22:46:08 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/cache.h>
#include <kernel/driver.h>
#include <kernel/memory.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#if 0
typedef struct cache_t cache_t;
struct cache_t
{
	device_t dev;
	device_t* blockdev;
	uint32_t block_size, cache_size;
	struct cacheblock_t
	{
		uint32_t block_num;
		uint8_t flags;
	} *blocks;

	uint8_t cache[0x4000];
};

#define BLOCK_USED	1

/*bool CcIsr(device_t *dev, uint8_t irq)
{
	return ((cache_t*) dev)->blockdev->isr(dev, irq);
}*/

bool CcRequest(device_t* dev, request_t* req)
{
	request_dev_t *req_dev = (request_dev_t*) req;
	cache_t* cache;
	uint32_t cell, block_num;
	size_t user_length, length;
	uint64_t ofs;
	
	cache = (cache_t*) dev;
	switch (req->code)
	{
	case DEV_READ:
		user_length = req_dev->params.dev_read.length;
		req_dev->params.dev_read.length = 0;

		while (req_dev->params.dev_read.length < user_length)
		{
			/*
			 * Round the offset down to the nearest multiple of the 
			 *	device's block size
			 */
			ofs = req_dev->params.dev_read.offset + req_dev->params.dev_read.length;
			ofs &= -cache->block_size;

			/* Calculate the index of the block on the device */
			block_num = (uint32_t) ofs / cache->block_size;
			/* Calculate the cell number of this cached block in the list */
			cell = block_num % cache->cache_size;

			/*wprintf(L"%d=>%d@%d: pos = %d offset = %d\n", 
				(uint32_t) req_dev->params.dev_read.offset, 
				block_num, cell, 
				req_dev->params.dev_read.offset + req_dev->params.dev_read.length,
				req_dev->params.dev_read.offset + req_dev->params.dev_read.length - ofs);*/

			/* Check for cache hit/miss based on the block cell number */
			if ((cache->blocks[cell].flags & BLOCK_USED) == 0 ||
				cache->blocks[cell].block_num != block_num)
			{
				/* Missed? Then load the whole cell from the device */

				/*_cputws_check(L"m");
				wprintf(L"miss\t");*/
				length = cache->block_size;
				assert(cell * cache->block_size + length <= sizeof(cache->cache));

				if (DevRead(cache->blockdev, 
					ofs,
					cache->cache + cell * cache->block_size,
					length) != length)
				{
					req->result = -1;
					return false;
				}

				cache->blocks[cell].flags |= BLOCK_USED;
				cache->blocks[cell].block_num = block_num;
			}
			/*else
				wprintf(L"hit\t");
				_cputws_check(L"h");*/

			length = min(cache->block_size, 
				user_length - req_dev->params.dev_read.length);
			memcpy((uint8_t*) req_dev->params.dev_read.buffer + req_dev->params.dev_read.length, 
				cache->cache + cell * cache->block_size + 
					req_dev->params.dev_read.offset + req_dev->params.dev_read.length - ofs,
					length);

			req_dev->params.dev_read.length += length;
		}

		return true;

	default:
		return cache->blockdev->vtbl->request(cache->blockdev, req);
	}
}

static const IDeviceVtbl cache_vtbl =
{
	CcRequest,
	NULL
};

device_t* CcInstallBlockCache(device_t* dev, uint32_t block_size)
{
	cache_t* cache;
	unsigned cache_size;

	cache_size = sizeof(cache->cache) / block_size;
	cache = malloc(sizeof(cache_t) + sizeof(struct cacheblock_t) * cache_size);
	assert(cache != NULL);

	cache->dev.vtbl = &cache_vtbl;
	cache->block_size = block_size;
	cache->blockdev = dev;
	cache->cache_size = cache_size;
	cache->blocks = (struct cacheblock_t*) (cache + 1);
	
	wprintf(L"CcInstallBlockCache: cached %d blocks\n", cache->cache_size);
	return &cache->dev;
}
#endif

typedef struct cache_page_t cache_page_t;
struct cache_page_t
{
	addr_t phys;
	bool is_valid;
};

cache_t *CcCreateFileCache(void)
{
	cache_t *cc;
	cc = malloc(sizeof(cache_t));
	SemInit(&cc->lock);
	cc->num_pages = 0;
	cc->pages = NULL;
	wprintf(L"CcCreateFileCache: %p\n", cc);
	return cc;
}

void CcDeleteFileCache(cache_t *cc)
{
	unsigned i;

	wprintf(L"CcDeleteFileCache(%p): %u pages\n", cc, cc->num_pages);
	SemAcquire(&cc->lock);
	for (i = 0; i < cc->num_pages; i++)
		if (cc->pages[i].phys != -1)
			MemFree(cc->pages[i].phys);

	free(cc->pages);
	free(cc);
}

void *CcRequestPage(cache_t *cc, uint64_t offset)
{
	unsigned page, i;

	SemAcquire(&cc->lock);
	page = PAGE_ALIGN(offset) / PAGE_SIZE;
	wprintf(L"CcRequestPage(%p): num_pages = %u offset = %u page = %u\n",
		cc, cc->num_pages, (uint32_t) offset, page);
	if (page > cc->num_pages)
	{
		cc->pages = realloc(cc->pages, sizeof(cache_page_t) * (page + 1));
		assert(cc->pages != NULL);

		for (i = cc->num_pages; i < page + 1; i++)
		{
			cc->pages[page].phys = -1;
			cc->pages[page].is_valid = false;
		}

		cc->num_pages = page + 1;
		cc->pages[page].phys = MemAlloc();
		wprintf(L"CcRequestPage: new page: phys = %x\n", 
			cc->pages[page].phys);
	}
	
	assert(cc->pages[page].phys != -1);
	MemLockPages(cc->pages[page].phys, 1, true);
	SemRelease(&cc->lock);
	return MemMapTemp(&cc->pages[page].phys, 1, 
		PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);
}

bool CcIsPageValid(cache_t *cc, uint64_t offset)
{
	unsigned page;

	page = PAGE_ALIGN(offset) / PAGE_SIZE;
	if (page > cc->num_pages)
		return false;
	else
		return cc->pages[page].is_valid;
}

void CcReleasePage(cache_t *cc, uint64_t offset, bool isValid)
{
	unsigned page;

	page = PAGE_ALIGN(offset) / PAGE_SIZE;
	if (page > cc->num_pages)
		return;
	
	SemAcquire(&cc->lock);
	cc->pages[page].is_valid = isValid;
	MemLockPages(cc->pages[page].phys, 1, false);
	MemUnmapTemp();
	SemRelease(&cc->lock);
}
