/* $Id: cache.c,v 1.12 2002/08/17 19:13:32 pavlovskii Exp $ */

/* xxx - what if block_size > PAGE_SIZE? */

#include <kernel/kernel.h>
#include <kernel/cache.h>
#include <kernel/driver.h>
#include <kernel/memory.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#define ALIGN(num, to)          ((num) & -(to))
#define ALIGN_UP(num, to)       (((num) + (to) - 1) & -(to))

typedef struct cache_block_t cache_block_t;
struct cache_block_t
{
    unsigned is_valid : 1;
    unsigned is_dirty : 1;
};

/*!
 *      \brief  Creates a file cache object
 *
 *      \param  block_size      The size of one cache block. This is usually the 
 *              file system's cluster size.
 *      \return A pointer to a file cache object
 */
cache_t *CcCreateFileCache(size_t block_size)
{
    cache_t *cc;
    unsigned temp;

    cc = malloc(sizeof(cache_t));
    SpinInit(&cc->lock);
    cc->block_size = block_size;

    temp = block_size;
    for (cc->block_shift = 0; (temp & 1) == 0; cc->block_shift++)
        temp >>= 1;

    cc->num_blocks = 0;
    cc->blocks = NULL;
    cc->num_pages = 0;
    cc->pages = NULL;
    /*wprintf(L"CcCreateFileCache(%p): block_size = %u block_shift = %u\n", 
        cc, cc->block_size, cc->block_shift);*/
    return cc;
}

/*!
 *      \brief  Deletes a file cache object
 *
 *      Any cached pages are flushed and freed.
 *      \param  cc      A pointer to the cache object
 */
void CcDeleteFileCache(cache_t *cc)
{
    unsigned i;

    /*wprintf(L"CcDeleteFileCache(%p): %u blocks, %u pages: ", 
        cc, cc->num_blocks, cc->num_pages);*/
    SpinAcquire(&cc->lock);
    for (i = 0; i < cc->num_pages; i++)
    {
        if (cc->pages[i] != -1)
        {
            MemFree(cc->pages[i]);
            //wprintf(L"!");
        }
        //else
            //wprintf(L".");
    }

    //wprintf(L"\n");
    free(cc->blocks);
    free(cc->pages);
    SpinRelease(&cc->lock);
    free(cc);
}

/*!
 *      \brief  Locks a cache block in memory, allocating it if necessary
 *
 *      The block will only contain valid data if \p CcIsBlockValid returned
 *      \p true before this function was called. This function does not
 *      change the validity of the block.
 *
 *      Because blocks are reference counted, \p CcReleaseBlock must be called 
 *      when you have finished with the block.
 *
 *      \param  cc      A pointer to the file cache object
 *      \param  offset  The offset of the block in the file
 *      \return A pointer to the start of the block in memory
 */

/*
 * xxx - this need to allocate multiple physical pages and return an array of 
 * their addresses
 */
page_array_t *CcRequestBlock(cache_t *cc, uint64_t offset)
{
    unsigned block, page_start, old, i, pages_per_block;

    SpinAcquire(&cc->lock);
    //mod = offset & ((1 << cc->block_shift) - 1);
    offset = ALIGN(offset, cc->block_size);
    block = offset >> cc->block_shift;
    page_start = (block * cc->block_size) / PAGE_SIZE;
    pages_per_block = PAGE_ALIGN_UP(cc->block_size) / PAGE_SIZE;
    /*wprintf(L"CcRequestBlock: offset = %u block = %u/%u + %u page_start = %u/%u pages = %u\n",
        (uint32_t) offset, 
        block, cc->num_blocks, mod,
        page_start, cc->num_pages,
        pages_per_block);*/
    //wprintf(L"CcRequestBlock(%u/%u): ", block, cc->num_blocks);
    if (block >= cc->num_blocks)
    {
        old = cc->num_blocks;
        cc->num_blocks = block + 1;
        cc->blocks = realloc(cc->blocks, sizeof(cache_block_t) * cc->num_blocks);
        assert(cc->blocks != NULL);
        for (i = old; i < cc->num_blocks; i++)
            cc->blocks[i].is_valid = cc->blocks[i].is_dirty = false;

        old = cc->num_pages;
        cc->num_pages = PAGE_ALIGN_UP(cc->num_blocks * cc->block_size) / PAGE_SIZE;
        if (cc->num_pages > old)
        {
            cc->pages = realloc(cc->pages, sizeof(addr_t) * cc->num_pages);
            assert(cc->pages != NULL);
            for (i = old; i < cc->num_pages; i++)
                cc->pages[i] = -1;
        }
    }

    for (i = page_start; i < page_start + pages_per_block; i++)
    {
        if (cc->pages[i] == -1)
        {
            cc->pages[i] = MemAlloc();
            //wprintf(L"+");
        }

        //wprintf(L"%x ", cc->pages[i]);
    }

    //wprintf(L"\n");
    /*assert(page_start + pages_per_block <= cc->num_pages);
    for (i = page_start; i < page_start + pages_per_block; i++)
    {
        assert(cc->pages[i] != -1);
        MemLockPages(cc->pages[i], 1, true);
    }*/

    SpinRelease(&cc->lock);

    return MemDupPageArray(pages_per_block, 
        (block << cc->block_shift) % PAGE_SIZE, 
        cc->pages + page_start);
    /*ptr = MemMapTemp(cc->pages + page_start, pages_per_block, 
        PRIV_KERN | PRIV_RD | PRIV_WR | PRIV_PRES);
    if (ptr == NULL)
        return NULL;
    else
    {
        wprintf(L"CcRequestBlock: ptr = %p mod = %u\n",
            ptr, (block << cc->block_shift) % PAGE_SIZE);
        return ptr + (block << cc->block_shift) % PAGE_SIZE;
    }*/
}

/*!
 *      \brief  Returns \p true if the specified cache block is valid
 *
 *      A block is invalid if any of the following are true:
 *      - Its offset is beyond the extent of the cached data
 *      - Its memory has been paged out
 *      - It has never been made valid
 *
 *      To change the validity of a block, call \p CcRequestBlock followed by
 *      \p CcReleaseBlock.
 *
 *      \param  cc      A pointer to the file cache object
 *      \param  offset  The offset of the block in the file
 *      \return \p true if the block is valid
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

bool CcIsBlockDirty(cache_t *cc, uint64_t offset)
{
    unsigned block;

    block = ALIGN(offset, cc->block_size) >> cc->block_shift;
    if (block >= cc->num_blocks)
        return false;
    else
        return cc->blocks[block].is_valid && cc->blocks[block].is_dirty;
}

/*!
 *      \brief  Releases a cache block
 *
 *      Any memory associated with the block is made available for paging, and
 *      the validity of the block is changed as appropriate.
 *
 *      \param  cc      A pointer to the file cache object
 *      \param  offset  The offset of the block in the file
 *      \param  isValid Indicates whether the block's memory now contains valid
 *              data.
 */
void CcReleaseBlock(cache_t *cc, uint64_t offset, bool isValid, bool isDirty)
{
    unsigned block, page;

    offset = ALIGN(offset, cc->block_size);
    block = offset >> cc->block_shift;
    assert(block < cc->num_blocks);

    page = (block * cc->block_size) / PAGE_SIZE;
    SpinAcquire(&cc->lock);
    cc->blocks[block].is_valid = isValid;
    if (isValid)
        cc->blocks[block].is_dirty = isDirty;
    else
        cc->blocks[block].is_dirty = false;

    /*if (cc->pages[page] != -1)
        MemLockPages(cc->pages[page], 1, false);

    MemUnmapTemp();*/
    SpinRelease(&cc->lock);
}
