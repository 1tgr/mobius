/* $Id: maldebug.c,v 1.1 2002/05/12 00:30:34 pavlovskii Exp $ */

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

#undef malloc
#undef free
#undef realloc

typedef struct maldbg_header_t maldbg_header_t;
struct maldbg_header_t
{
    int line;
    const char *file;
    maldbg_header_t *prev, *next;
    int tag;
    unsigned int magic[2];
};

static maldbg_header_t *maldbg_first, *maldbg_last;
static int maldbg_tag;

void *__malloc(size_t size, const char *file, int line)
{
    maldbg_header_t *header;

    header = malloc(sizeof(maldbg_header_t) + size);
    if (header == NULL)
        return NULL;

    header->line = line;
    header->file = file;
    header->prev = maldbg_last;
    header->tag = maldbg_tag;
    header->magic[0] = 0xdeadbeef;
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
    maldbg_header_t *header;

    if (ptr != NULL)
    {
        header = (maldbg_header_t*) ptr - 1;

        if (header->prev != NULL)
            header->prev->next = header->next;
        if (header->next != NULL)
            header->next->prev = header->prev;
        if (maldbg_last == header)
            maldbg_last = header->prev;
        if (maldbg_first == header)
            maldbg_first = header->next;

        free(header);
    }
}

void *__realloc(void *ptr, size_t size, const char *file, int line)
{
    maldbg_header_t *header;

    if (ptr == NULL)
        return __malloc(size, file, line);
    else
    {
        header = (maldbg_header_t*) ptr - 1;
        header = realloc(header, sizeof(maldbg_header_t) + size);

        if (header->next == NULL)
            maldbg_last = header;
        else
            header->next->prev = header;

        if (header->prev == NULL)
            maldbg_first = header;
        else
            header->prev->next = header;

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

void __malloc_leak_dump(void)
{
    maldbg_header_t *header;

    for (header = maldbg_first; header != NULL; header = header->next)
    {
        if (header->magic[0] != 0xdeadbeef)
            wprintf(L"%p: memory check error (1): got 0x%x, should be 0xdeadbeef\n",
                header + 1, header->magic[0]);

        if (header->magic[1] != (header->magic[0] ^ header->line))
            wprintf(L"%p: memory check error (2): got 0x%x, should be 0x%x\n",
                header + 1, header->magic[1], header->magic[0] ^ header->line);

        if (header->tag == maldbg_tag)
            wprintf(L"%S(%d): memory leaked: %p\n", 
                header->file, header->line, header + 1);
    }
}
