/* $Id$ */
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"

typedef struct file_info_t file_info_t;
struct file_info_t
{
    dirent_t dirent;
    dirent_standard_t standard;
};

static int ShCompareDirent(const void *a, const void *b)
{
    const file_info_t *da, *db;

    da = a;
    db = b;
    if((da->standard.attributes & FILE_ATTR_DIRECTORY) &&
       (db->standard.attributes & FILE_ATTR_DIRECTORY) == 0)
        return -1;
    else if ((db->standard.attributes & FILE_ATTR_DIRECTORY) &&
             (da->standard.attributes & FILE_ATTR_DIRECTORY) == 0)
        return 1;
    else
        return _wcsicmp(((const dirent_t*) a)->name, 
            ((const dirent_t*) b)->name);
}

static void ShListDirectory(wchar_t *path, const wchar_t *spec, unsigned indent, 
                            bool do_recurse, bool is_full)
{
    handle_t search;
    file_info_t *entries;
    wchar_t *end;
    unsigned num_entries, alloc_entries, i, n, columns;
    size_t max_len, len;

    end = path + wcslen(path);
    if (end[-1] != '/')
    {
        wcscpy(end, L"/");
        end++;
    }
    //end++;

    //KeLeakBegin();
    search = FsOpenDir(path);

    if (search != NULL)
    {
        entries = malloc(sizeof(file_info_t));
        num_entries = alloc_entries = 0;
        max_len = 0;
        while (FsReadDir(search, &entries[num_entries].dirent, sizeof(dirent_t)))
        {
            len = wcslen(entries[num_entries].dirent.name) + 2;
            if (len > max_len)
                max_len = len;

            wcscpy(end, entries[num_entries].dirent.name);
            if (!FsQueryFile(path, 
                FILE_QUERY_STANDARD, 
                &entries[num_entries].standard, 
                sizeof(dirent_standard_t)))
            {
                _pwerror(path);
                continue;
            }

            num_entries++;
            if (num_entries >= alloc_entries)
            {
                alloc_entries = num_entries + 16;
                //wprintf(L"ShListDirectory: alloc_entries = %u\n", alloc_entries);
                entries = realloc(entries, sizeof(file_info_t) * (alloc_entries + 1));
            }
        }

        HndClose(search);

        qsort(entries, num_entries, sizeof(file_info_t), ShCompareDirent);
        if (max_len == 0)
            columns = 1;
        else
            columns = (sh_width - 2) / max_len - 1;
        for (i = n = 0; i < num_entries; i++)
        {
            if (do_recurse)
            {
                if (entries[i].standard.attributes & FILE_ATTR_DIRECTORY)
                {
                    printf("%*s+ %S\n", indent * 2, "", entries[i].dirent.name);                
                    ShListDirectory(path, spec, indent + 1, true, is_full);
                }
                else if (_wcsmatch(spec, entries[i].dirent.name) == 0)
                    printf("%*s  %S\n", indent * 2, "", entries[i].dirent.name);
            }
            else if (_wcsmatch(spec, entries[i].dirent.name) == 0)
            {
                if (is_full)
                {
                    if (entries[i].standard.attributes & FILE_ATTR_DIRECTORY)
                        printf("[Directory]\t");
                    else if (entries[i].standard.attributes & FILE_ATTR_DEVICE)
                        printf("   [Device]\t");
                    else
                        printf("%10lu\t", (uint32_t) entries[i].standard.length);

                    printf("\t%S", entries[i].dirent.name);

                    printf("%*s%S\n", 
                        20 - wcslen(entries[i].dirent.name) + 1, "", 
                        entries[i].standard.mimetype);
                }
                else
                {
                    if (entries[i].standard.attributes & FILE_ATTR_DIRECTORY)
                        printf("\x1b[1m");

                    printf("%S%*s", entries[i].dirent.name, 
                        max_len - wcslen(entries[i].dirent.name), "");

                    if (entries[i].standard.attributes & FILE_ATTR_DIRECTORY)
                        printf("\x1b[m");

                    if (n == columns)
                    {
                        printf("\n");
                        n = 0;
                    }
                    else
                        n++;
                }
            }
        }

        if (!do_recurse && !is_full && (n - 1) != columns)
            printf("\n");
        free(entries);
    }
    else
        _pwerror(path);

    //KeLeakEnd();
}

void ShCmdDir(const wchar_t *command, wchar_t *params)
{
    wchar_t **names, **values, *spec;

    ShParseParams(params, &names, &values, &params);

    spec = wcsrchr(params, '/');
    if (spec == NULL)
    {
        spec = params;
        params = L".";
    }
    else
    {
        *spec = '\0';
        spec++;
    }

    if (*spec == '\0')
        spec = L"*";
    if (*params == '\0')
        params = L".";

    if (!FsFullPath(params, sh_path))
    {
        _pwerror(params);
        return;
    }

    ShListDirectory(sh_path, spec, 0, 
        ShHasParam(names, L"s"), 
        ShHasParam(names, L"full"));

    free(names);
    free(values);
}
