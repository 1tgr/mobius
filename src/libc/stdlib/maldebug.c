/* $Id: maldebug.c,v 1.4 2002/12/18 23:13:31 pavlovskii Exp $ */

#include <malloc.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

#undef malloc
#undef free
#undef realloc

static __maldbg_header_t *maldbg_first, *maldbg_last;
static int maldbg_tag;

#define MAL_MAGIC   0xdeadbeef

void *__malloc(size_t size, const char *file, int line)
{
    __maldbg_header_t *header;

    header = malloc(sizeof(__maldbg_header_t) + size);
    if (header == NULL)
        return NULL;

    header->line = line;
    header->file = file;
    header->prev = maldbg_last;
    header->size = sizeof(__maldbg_header_t) + size;
    header->tag = maldbg_tag;
    header->magic[0] = MAL_MAGIC;
    header->magic[1] = line ^ header->magic[0];
    header->next = NULL;

    if (maldbg_last != NULL)
        maldbg_last->next = header;

    if (maldbg_first == NULL)
        maldbg_first = header;

    maldbg_last = header;

    return header + 1;
}

void __free(void *ptr, const char *file, int line)
{
    __maldbg_header_t *header;

    if (ptr != NULL)
    {
        header = (__maldbg_header_t*) ptr - 1;

        if (header->prev != NULL)
            header->prev->next = header->next;
        if (header->next != NULL)
            header->next->prev = header->prev;
        if (maldbg_last == header)
            maldbg_last = header->prev;
        if (maldbg_first == header)
            maldbg_first = header->next;

        header->tag = 0;
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
        header = realloc(header, sizeof(__maldbg_header_t) + size);

        if (header->next == NULL)
            maldbg_last = header;
        else
            header->next->prev = header;

        if (header->prev == NULL)
            maldbg_first = header;
        else
            header->prev->next = header;

        header->size = sizeof(__maldbg_header_t) + size;
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

void __malloc_leak(int tag)
{
    maldbg_tag = tag;
}

bool __malloc_check_header(__maldbg_header_t *header)
{
    if (header->magic[0] != 0xdeadbeef)
    {
        wprintf(L"%p: memory check error (1): got 0x%x, should be 0xdeadbeef\n",
            header + 1, header->magic[0]);
        return false;
    }

    if (header->magic[1] != (header->magic[0] ^ header->line))
    {
        wprintf(L"%p: memory check error (2): got 0x%x, should be 0x%x\n",
            header + 1, header->magic[1], header->magic[0] ^ header->line);
        return false;
    }

    return true;
}

void __malloc_leak_dump(void)
{
    __maldbg_header_t *header;

    for (header = maldbg_first; header != NULL; header = header->next)
        if (__malloc_check_header(header) &&
            header->tag == maldbg_tag)
            wprintf(L"%S(%d): memory leaked: %p\n", 
                header->file, header->line, header + 1);
}

__maldbg_header_t *__malloc_find_block(void *addr)
{
    __maldbg_header_t *header;

    for (header = maldbg_first; header != NULL; header = header->next)
        if ((__maldbg_header_t*) addr >= header &&
            __malloc_check_header(header) &&
            (uint8_t*) addr < (uint8_t*) header + header->size)
            return header;

    return NULL;
}
