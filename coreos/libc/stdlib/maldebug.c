/* $Id: maldebug.c,v 1.1.1.1 2002/12/21 09:50:10 pavlovskii Exp $ */

#include <malloc.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <os/defs.h>
#include <os/rtl.h>
#include "init.h"

#undef malloc
#undef free
#undef realloc

static __maldbg_header_t *maldbg_first, *maldbg_last;
static int maldbg_tag;
static lmutex_t maldbg_mux;

#define MAL_MAGIC   0xdeadbeef

static __maldbg_header_t *__malloc_find_block(void *addr)
{
    __maldbg_header_t *header;

    for (header = maldbg_first; header != NULL; header = header->next)
        if ((__maldbg_header_t*) addr == header + 1)
            break;

    return header;
}


bool __malloc_check_header(__maldbg_header_t *header)
{
    if (header->magic[0] != 0xdeadbeef)
    {
        _wdprintf(L"%p: memory check error (1): got 0x%x, should be 0xdeadbeef\n",
            header + 1, header->magic[0]);
        return false;
    }

    if (header->magic[1] != (header->magic[0] ^ header->size))
    {
        _wdprintf(L"%p: memory check error (2): got 0x%x, should be 0x%x\n",
            header + 1, header->magic[1], header->magic[0] ^ header->size);
        return false;
    }

	if (__malloc_find_block(header + 1) == NULL)
	{
		_wdprintf(L"%p: memory block not found in list\n", header + 1);
		return false;
	}

    return true;
}

void *__malloc(size_t size, const char *file, int line)
{
    __maldbg_header_t *header;

    header = malloc(sizeof(__maldbg_header_t) + size);
    if (header == NULL)
        return NULL;

	LmuxAcquire(&maldbg_mux);
    header->line = line;
    header->file = file;
    header->prev = maldbg_last;
    header->size = sizeof(__maldbg_header_t) + size;
    header->tag = maldbg_tag;
    header->magic[0] = MAL_MAGIC;
    header->magic[1] = header->magic[0] ^ header->size;
    header->next = NULL;

    if (maldbg_last != NULL)
        maldbg_last->next = header;

    if (maldbg_first == NULL)
        maldbg_first = header;

    maldbg_last = header;
	LmuxRelease(&maldbg_mux);

	memset(header + 1, 0xcd, size);
    return header + 1;
}

void __free(void *ptr, const char *file, int line)
{
    __maldbg_header_t *header;

    if (ptr != NULL)
    {
        header = (__maldbg_header_t*) ptr - 1;

		LmuxAcquire(&maldbg_mux);
		assert(__malloc_check_header(header));
        if (header->prev != NULL)
            header->prev->next = header->next;
        if (header->next != NULL)
            header->next->prev = header->prev;
        if (maldbg_last == header)
            maldbg_last = header->prev;
        if (maldbg_first == header)
            maldbg_first = header->next;
		LmuxRelease(&maldbg_mux);

		memset(header, 0xdd, header->size);
        free(header);
    }
}

void *__realloc(void *ptr, size_t size, const char *file, int line)
{
    __maldbg_header_t *header;

    if (ptr == NULL)
        return __malloc(size, file, line);
    else
    {
        header = (__maldbg_header_t*) ptr - 1;

		LmuxAcquire(&maldbg_mux);
		assert(__malloc_check_header(header));

        header = realloc(header, sizeof(__maldbg_header_t) + size);

        if (header->next == NULL)
            maldbg_last = header;
        else
            header->next->prev = header;

        if (header->prev == NULL)
            maldbg_first = header;
        else
            header->prev->next = header;
		LmuxRelease(&maldbg_mux);

		header->size = sizeof(__maldbg_header_t) + size;
		header->magic[1] = header->magic[0] ^ header->size;
		assert(__malloc_check_header(header));
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
        if (__malloc_check_header(header) &&
            header->tag == maldbg_tag)
            _wdprintf(L"%S(%d): memory leaked: %p\n", 
                header->file, header->line, header + 1);
}

static void __malloc_debug_cleanup(void)
{
	LmuxDelete(&maldbg_mux);
}


void __malloc_debug_init(void)
{
	LmuxInit(&maldbg_mux);
	atexit(__malloc_debug_cleanup);
}
