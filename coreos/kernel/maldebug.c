/*
 * $History: maldebug.c $
 * 
 * *****************  Version 4  *****************
 * User: Tim          Date: 13/03/04   Time: 22:20
 * Updated in $/coreos/kernel
 * Commented calls to __malloc_check_all
 * 
 * *****************  Version 3  *****************
 * User: Tim          Date: 7/03/04    Time: 13:46
 * Updated in $/coreos/kernel
 * Align footers to 4 bytes
 * Fill expanded blocks in realloc
 * Added test function
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 14:03
 * Updated in $/coreos/kernel
 * Added malloc block footers, with their own magic numbers
 * Added history block
 */

#include <kernel/kernel.h>
#include <kernel/memory.h>

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

#include <os/defs.h>

__maldbg_header_t *maldbg_first, *maldbg_last;

static int maldbg_tag;
static spinlock_t maldbg_mux;

#define MAL_MAGIC   0xabababab
#define MAL_MAGIC_2 0xefefefef


typedef struct __maldbg_footer_t __maldbg_footer_t;
struct __maldbg_footer_t
{
    unsigned magic[2];
};


void *  (malloc)(size_t _size);
void    (free)(void *_ptr);
void *  (realloc)(void *_ptr, size_t _size);

static bool __malloc_check_magic(__maldbg_header_t *header)
{
    __maldbg_footer_t *footer;

    if (header->magic[0] != MAL_MAGIC)
    {
        wprintf(L"%p: memory check error (1): got 0x%x, should be 0x%x\n",
            header + 1, header->magic[0], MAL_MAGIC);
        return false;
    }

    if (header->magic[1] != (header->magic[0] * header->size))
    {
        wprintf(L"%p: memory check error (2): got 0x%x, should be 0x%x\n",
            header + 1, header->magic[1], header->magic[0] * header->size);
        return false;
    }

    footer = (__maldbg_footer_t*) ((uint8_t*) header + header->size);
    if (footer->magic[0] != MAL_MAGIC_2)
    {
        wprintf(L"%p: memory check error (3): got 0x%x, should be 0x%x\n",
            header + 1, footer->magic[0], MAL_MAGIC_2);
        return false;
    }

    if (footer->magic[1] != (footer->magic[0] * header->size))
    {
        wprintf(L"%p: memory check error (4): got 0x%x, should be 0x%x\n",
            header + 1, footer->magic[1], footer->magic[0] * header->size);
        return false;
    }

    return true;
}


bool __malloc_check_next(__maldbg_header_t *header)
{
    if (header->next != NULL &&
        (MemTranslate(header->next) & PRIV_PRES) == 0)
    {
        assert(__malloc_check_magic(header));
        wprintf(L"__malloc_check_next(%p): bad next pointer: %p\n",
            header,
            header->next);
        wprintf(L"\tAllocated at %S(%d)\n",
            header->file, header->line);
        return false;
    }
    else
        return true;
}


__maldbg_header_t *__malloc_find_block(void *addr)
{
    __maldbg_header_t *header;

    for (header = maldbg_first; header != NULL; header = header->next)
    {
        assert(__malloc_check_next(header));
        if ((uint8_t*) addr >= (uint8_t*) header &&
            (uint8_t*) addr < (uint8_t*) header + header->size + sizeof(__maldbg_footer_t))
            return header;
    }

    return NULL;
}


bool __malloc_check_block(__maldbg_header_t *header)
{
    if (!__malloc_check_magic(header))
        return false;

    if (__malloc_find_block(header + 1) == NULL)
    {
        wprintf(L"%p: memory block not found in list\n", header + 1);
        return false;
    }

    return true;
}


bool __malloc_check_all(void)
{
    __maldbg_header_t *header;

    for (header = maldbg_first; header != NULL; header = header->next)
    {
        assert(__malloc_check_next(header));
        if (!__malloc_check_magic(header))
            return false;
    }

    return true;
}


static size_t __malloc_align(size_t size)
{
    assert(((size + 3) & -4) >= size);
    return (size + 3) & -4;
}


void *__malloc(size_t size, const char *file, int line)
{
    __maldbg_header_t *header;
    __maldbg_footer_t *footer;

    //assert(__malloc_check_all());

    size = __malloc_align(size);
    header = (malloc)(sizeof(*header) + size + sizeof(*footer));
    if (header == NULL)
        return NULL;

    memset(header, 0, sizeof(*header));
    header->line = line;
    header->file = file;
    header->size = sizeof(*header) + size;
    header->tag = maldbg_tag;
    header->magic[0] = MAL_MAGIC;
    header->magic[1] = header->magic[0] * header->size;

    footer = (__maldbg_footer_t*) ((uint8_t*) header + header->size);
    footer->magic[0] = MAL_MAGIC_2;
    footer->magic[1] = footer->magic[0] * header->size;

    SpinAcquire(&maldbg_mux);
    LIST_ADD(maldbg, header);
    assert(MemTranslate(maldbg_last) & PRIV_PRES);
    SpinRelease(&maldbg_mux);

    memset(header + 1, 0xcd, size);
    return header + 1;
}


void __free(void *ptr, const char *file, int line)
{
    __maldbg_header_t *header;

    //assert(__malloc_check_all());

    if (ptr != NULL)
    {
        header = (__maldbg_header_t*) ptr - 1;

        SpinAcquire(&maldbg_mux);
        assert(MemTranslate(maldbg_last) & PRIV_PRES);
        assert(__malloc_check_block(header));
        LIST_REMOVE(maldbg, header);
        SpinRelease(&maldbg_mux);

        memset(header, 0xdd, header->size);
        (free)(header);
    }
}


void *__realloc(void *ptr, size_t size, const char *file, int line)
{
    __maldbg_header_t *header;
    __maldbg_footer_t *footer;

    //assert(__malloc_check_all());

    size = __malloc_align(size);
    if (ptr == NULL)
        return __malloc(size, file, line);
    else
    {
        unsigned old_size;

        header = (__maldbg_header_t*) ptr - 1;

        SpinAcquire(&maldbg_mux);
        assert(MemTranslate(maldbg_last) & PRIV_PRES);
        assert(__malloc_check_block(header));
        LIST_REMOVE(maldbg, header);
        SpinRelease(&maldbg_mux);

        old_size = header->size;
        header = (realloc)(header, sizeof(*header) + size + sizeof(*footer));

        SpinAcquire(&maldbg_mux);
        LIST_ADD(maldbg, header);
        assert(__malloc_check_block(header));
        SpinRelease(&maldbg_mux);

        header->size = sizeof(*header) + size;
        header->magic[1] = header->magic[0] * header->size;

        if (header->size > old_size)
            memset((uint8_t*) header + old_size, 0xcd, header->size - old_size);

        footer = (__maldbg_footer_t*) ((uint8_t*) header + header->size);
        footer->magic[0] = MAL_MAGIC_2;
        footer->magic[1] = footer->magic[0] * header->size;
        return header + 1;
    }
}


wchar_t *__wcsdup(const wchar_t* str, const char *file, int line)
{
    wchar_t *ret;

    if (str == NULL)
        str = L"";

    ret = __malloc((wcslen(str) + 1) * sizeof(wchar_t), file, line);

    if (!ret)
        return NULL;

    wcscpy(ret, str);
    return ret;
}


char *__strdup(const char* str, const char *file, int line)
{
    char *ret;

    if (str == NULL)
        str = "";

    ret = __malloc(strlen(str) + 1, file, line);

    if (!ret)
        return NULL;

    strcpy(ret, str);
    return ret;
}


void __malloc_leak(int tag)
{
    maldbg_tag = tag;
}


void __malloc_leak_dump(void)
{
    __maldbg_header_t *header;

    for (header = maldbg_first; header != NULL; header = header->next)
        if (__malloc_check_magic(header) &&
            header->tag == maldbg_tag)
            wprintf(L"%S(%d): memory leaked: %p\n", 
            header->file, header->line, header + 1);
}


void __malloc_test(void)
{
    void *block;
    __maldbg_header_t *header;

    wprintf(L"---- malloc_test ----\n");
    wprintf(L"Allocating 10 bytes: ");
    block = malloc(10);
    wprintf(L"%p\n");

    wprintf(L"Reallocing to 1000 bytes: ");
    block = realloc(block, 1000);
    wprintf(L"%p\n");

    wprintf(L"Freeing\n");
    free(block);

    wprintf(L"Allocating again: ");
    block = malloc(10);
    wprintf(L"%p\n");

    wprintf(L"Finding header: ");
    header = __malloc_find_block(block);
    wprintf(L"%p\n");

    assert(header == (__maldbg_header_t*) block - 1);

    wprintf(L"Checking block\n");
    assert(__malloc_check_block(header));

    wprintf(L"Creating buffer overrun\n");
    strcpy(block, "Long string");

    wprintf(L"Checking block is broken\n");
    assert(!__malloc_check_block(header));

    wprintf(L"Creating buffer underrun\n");
    strcpy((char*) block - 3, "Longer string");

    wprintf(L"Checking block is still broken\n");
    assert(!__malloc_check_block(header));

    LIST_REMOVE(maldbg, header);
    (free)(header);

    wprintf(L"---- finished malloc_test ----\n");
}
