/*
 * $History: devices.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 14/03/04   Time: 0:31
 * Updated in $/coreos/monitor
 * Attempt to mount as FAT if ext2 fails
 * Added history block
 */

#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>

#include <os/defs.h>
#include <os/syscall.h>
#include <os/rtl.h>

typedef struct dirent_list_t dirent_list_t;
struct dirent_list_t
{
    unsigned alloc_entries;
    unsigned num_entries;
    dirent_t *entries;
};


static int DirCompareDirent(const void *a, const void *b)
{
    const dirent_t *da, *db;
    da = a;
    db = b;
    return _wcsicmp(da->name, db->name);
}


dirent_list_t *DirCreateList(const wchar_t *path)
{
    dirent_list_t *ret;
    dirent_t di;
    handle_t dir;

    dir = FsOpenDir(path);
    if (dir == 0)
        return NULL;

    ret = malloc(sizeof(*ret));
    if (ret == NULL)
    {
        HndClose(dir);
        return NULL;
    }

    ret->alloc_entries = 0;
    ret->num_entries = 0;
    ret->entries = NULL;

    while (FsReadDir(dir, &di, sizeof(di)))
    {
        if (ret->num_entries >= ret->alloc_entries)
        {
            if (ret->alloc_entries == 0)
                ret->alloc_entries = 1;

            while (ret->alloc_entries <= ret->num_entries)
                ret->alloc_entries *= 2;

            ret->entries = realloc(ret->entries, 
                ret->alloc_entries * sizeof(*ret->entries));
            if (ret->entries == NULL)
            {
                free(ret);
                HndClose(dir);
                return NULL;
            }
        }

        ret->entries[ret->num_entries] = di;
        ret->num_entries++;
    }

    HndClose(dir);
    qsort(ret->entries, ret->num_entries, sizeof(*ret->entries), DirCompareDirent);
    return ret;
}


void DirDeleteList(dirent_list_t *list)
{
    free(list->entries);
    free(list);
}


static void MonitorDeviceAdded(const wchar_t *name)
{
    dirent_device_t info;
    wchar_t path[MAX_PATH] = { L"/Drives/" }, *path_ptr;
    wchar_t dest[MAX_PATH] = { L"Classes/" }, *dest_ptr;

    path_ptr = path + wcslen(path);
    dest_ptr = dest + wcslen(dest);

    if (!FsQueryFile(name, FILE_QUERY_DEVICE, &info, sizeof(info)))
        wcscpy(info.description, L"<unknown>");

    _wdprintf(L"MonitorDeviceAdded: %s '%s'\n", name, info.description);

    wcscpy(path_ptr, name);
    wcscpy(dest_ptr, name);

    switch (info.device_class)
    {
    case 0x0081:
    case 0x0102:
        _wdprintf(L"\tAttempting to mount volume\n");
        FsCreateDir(path);
        if (!FsMount(path, L"ext2", dest))
            FsMount(path, L"fat", dest);
        break;

    case 0x0082:
        _wdprintf(L"\tAttempting to mount CD-ROM\n");
        FsCreateDir(path);
        FsMount(path, L"cdfs", dest);
        break;
    }
}


static void MonitorDeviceRemoved(const wchar_t *name)
{
    _wdprintf(L"MonitorDeviceRemoved: %s\n", name);
}


int MonitorDeviceFinderThread(void *param)
{
    dirent_list_t *entries_old, *entries_new;

    FsChangeDir(SYS_DEVICES L"/Classes");
    FsCreateDir(L"/Drives");
    entries_old = NULL;
    while (true)
    {
        entries_new = DirCreateList(SYS_DEVICES L"/Classes");
        if (entries_new == NULL)
        {
            ThrSleep(500);
            continue;
        }

        if (entries_old != NULL)
        {
            unsigned index_old, index_new;

            index_old = index_new = 0;
            while (index_old < entries_old->num_entries ||
                index_new < entries_new->num_entries)
            {
                int comparison;

                if (index_old >= entries_old->num_entries)
                    comparison = 1;
                else if (index_new >= entries_new->num_entries)
                    comparison = -1;
                else
                    comparison = DirCompareDirent(entries_old->entries + index_old,
                        entries_new->entries + index_new);

                if (comparison < 0)
                {
                    /* new > old: item removed from new */
                    MonitorDeviceRemoved(entries_old->entries[index_old].name);
                    index_old++;
                }
                else if (comparison > 0)
                {
                    /* old > new: item added to new */
                    MonitorDeviceAdded(entries_new->entries[index_new].name);
                    index_new++;
                }
                else
                {
                    /* old == new: items the same */
                    index_old++;
                    index_new++;
                }
            }

            DirDeleteList(entries_old);
        }
        else
        {
            unsigned i;

            for (i = 0; i < entries_new->num_entries; i++)
                MonitorDeviceAdded(entries_new->entries[i].name);
        }

        entries_old = entries_new;

        ThrSleep(15000);
    }
}
