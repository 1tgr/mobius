/* $Id: unicode.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <wchar.h>
#include <stdlib.h>
#include <os/syscall.h>
#include <os/defs.h>

static handle_t uc_file;
static dirent_standard_t uc_di;
static __wchar_info_t *uc_unicode;

extern wchar_t __uc_map_file[];

static int do_lookup_unicode(const void *key, const void *value)
{
    const wchar_t *k;
    const __wchar_info_t *v;
    k = key;
    v = value;
    return *k - v->__cp;
}

const __wchar_info_t *__lookup_unicode(wchar_t cp)
{
    if (uc_file == NULL)
    {
        uc_file = FsOpen(__uc_map_file, FILE_READ);

        if (uc_file == NULL)
            return NULL;

        FsQueryHandle(uc_file, FILE_QUERY_STANDARD, &uc_di, sizeof(uc_di));
        uc_unicode = VmmMapFile(uc_file, 
            NULL, 
            PAGE_ALIGN_UP(uc_di.length) / PAGE_SIZE, 
            VM_MEM_USER | VM_MEM_READ);
    }

    return bsearch(&cp, 
        uc_unicode, 
        uc_di.length / sizeof(__wchar_info_t), 
        sizeof(__wchar_info_t), 
        do_lookup_unicode);
}
