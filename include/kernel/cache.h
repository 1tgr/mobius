/* $Id: cache.h,v 1.2 2002/01/07 00:14:05 pavlovskii Exp $ */
#ifndef __KERNEL_CACHE_H
#define __KERNEL_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

/*struct device_t;

struct device_t *CcInstallBlockCache(struct device_t *dev, uint32_t block_size);*/

typedef struct cache_t cache_t;
struct cache_t
{
	semaphore_t lock;
	size_t block_size;
	unsigned block_shift;
	unsigned num_blocks;
	struct cache_block_t *blocks;
	unsigned num_pages;
	addr_t *pages;
};

cache_t *	CcCreateFileCache(size_t block_size);
void		CcDeleteFileCache(cache_t *cc);
void *		CcRequestBlock(cache_t *cc, uint64_t offset);
bool		CcIsBlockValid(cache_t *cc, uint64_t offset);
void		CcReleaseBlock(cache_t *cc, uint64_t offset, bool isValid);

#ifdef __cplusplus
}
#endif

#endif