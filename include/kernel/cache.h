/* $Id: cache.h,v 1.5 2002/08/17 23:09:01 pavlovskii Exp $ */
#ifndef __KERNEL_CACHE_H
#define __KERNEL_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 *	\ingroup	kernel
 *	\defgroup	cc	Cache
 *	@{
 */

typedef struct cache_t cache_t;
/*! \brief	Represents a file cache object */
struct cache_t
{
    spinlock_t lock;
    size_t block_size;
    unsigned block_shift;
    unsigned num_blocks;
    struct cache_block_t *blocks;
    unsigned num_pages;
    addr_t *pages;
};

struct page_array_t;

cache_t *   CcCreateFileCache(size_t block_size);
void        CcDeleteFileCache(cache_t *cc);
struct page_array_t *CcRequestBlock(cache_t *cc, uint64_t offset);
bool        CcIsBlockValid(cache_t *cc, uint64_t offset);
bool        CcIsBlockDirty(cache_t *cc, uint64_t offset);
void        CcReleaseBlock(cache_t *cc, uint64_t offset, bool isValid, bool isDirty);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif