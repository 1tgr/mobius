/* $Id: cache.c,v 1.1 2002/01/05 00:58:35 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/cache.h>
#include <kernel/driver.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

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

bool CcIsr(device_t *dev, uint8_t irq)
{
	return ((cache_t*) dev)->blockdev->isr(dev, irq);
}

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
		return cache->blockdev->request(cache->blockdev, req);
	}
}

device_t* CcInstallBlockCache(device_t* dev, uint32_t block_size)
{
	cache_t* cache;
	unsigned cache_size;

	cache_size = sizeof(cache->cache) / block_size;
	cache = malloc(sizeof(cache_t) + sizeof(struct cacheblock_t) * cache_size);
	assert(cache != NULL);

	cache->dev.request = CcRequest;

	if (dev->isr)
		cache->dev.isr = CcIsr;
	else
		cache->dev.isr = NULL;

	cache->dev.finishio = NULL;
	cache->block_size = block_size;
	cache->blockdev = dev;
	cache->cache_size = cache_size;
	cache->blocks = (struct cacheblock_t*) (cache + 1);
	
	wprintf(L"CcInstallBlockCache: cached %d blocks\n", cache->cache_size);
	return &cache->dev;
}