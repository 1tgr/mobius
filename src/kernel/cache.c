#include <kernel/kernel.h>
#include <kernel/cache.h>
#include <stdlib.h>
#include <string.h>

typedef struct cache_t cache_t;
struct cache_t
{
	device_t dev;
	device_t* blockdev;
	dword block_size, cache_size;
	struct cacheblock_t
	{
		dword block_num;
		byte flags;
	} *blocks;

	byte cache[0x4000];
};

#define BLOCK_USED	1

bool ccRequest(device_t* dev, request_t* req)
{
	cache_t* cache;
	dword pos, block_num;
	size_t length;
	status_t hr;
	
	cache = (cache_t*) dev;
	switch (req->code)
	{
	case DEV_READ:
		req->user_length = req->params.read.length;
		req->params.read.length = 0;

		while (req->params.read.length < req->user_length)
		{
			block_num = (dword) (req->params.read.pos + req->params.read.length) / 
				cache->block_size;
			pos = block_num % cache->cache_size;

			//wprintf(L"%d=>%d@%d: ", (dword) req->params.read.pos, 
				//block_num, pos);

			if ((cache->blocks[pos].flags & BLOCK_USED) == 0 ||
				cache->blocks[pos].block_num != block_num)
			{
				_cputws_check(L"m");
				//wprintf(L"miss\t");
				length = cache->block_size;
				assert(pos * cache->block_size + length <= sizeof(cache->cache));
				
				hr = devReadSync(cache->blockdev, 
					req->params.read.pos + req->params.read.length,
					cache->cache + pos * cache->block_size,
					&length);
				if (hr != 0)
				{
					req->result = hr;
					return false;
				}

				cache->blocks[pos].flags |= BLOCK_USED;
				cache->blocks[pos].block_num = block_num;
			}
			else
				//wprintf(L"hit\t");
				_cputws_check(L"h");

			length = min(cache->block_size, 
				req->user_length - req->params.read.length);
			memcpy((byte*) req->params.read.buffer + req->params.read.length, 
				cache->cache + pos * cache->block_size,
				length);

			req->params.read.length += length;
		}

		hndSignal(req->event, true);
		return true;
	default:
		return cache->blockdev->request(cache->blockdev, req);
	}
}

device_t* ccInstallBlockCache(device_t* dev, dword block_size)
{
	cache_t* cache;

	cache = hndAlloc(sizeof(cache_t), NULL);
	assert(cache != NULL);

	cache->dev.request = ccRequest;
	cache->block_size = block_size;
	cache->blockdev = dev;
	cache->cache_size = sizeof(cache->cache) / block_size;
	cache->blocks = malloc(sizeof(struct cacheblock_t) * cache->cache_size);
	assert(cache->blocks != NULL);

	wprintf(L"ccInstallBlockCache: cached %d blocks\n", cache->cache_size);
	return &cache->dev;
}