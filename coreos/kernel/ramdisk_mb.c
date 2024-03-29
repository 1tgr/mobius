/* $Id: ramdisk_mb.c,v 1.1.1.1 2002/12/21 09:49:28 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/fs.h>
#include <kernel/init.h>
#include <kernel/multiboot.h>

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <string.h>

extern process_t proc_idle;

status_t RdParseElement(fsd_t *fsd, const wchar_t *name, wchar_t **new_path, vnode_t *node)
{
    multiboot_module_t *mods;
    wchar_t wc_name[16];
    size_t len;
    char *ch;
    unsigned i;

    assert(node->id == VNODE_ROOT);
    mods = PHYSICAL(kernel_startup.multiboot_info->mods_addr);
	for (i = 0; i < kernel_startup.multiboot_info->mods_count; i++)
    {
		ch = strrchr(PHYSICAL(mods[i].string), '/');
        if (ch == NULL)
            ch = PHYSICAL(mods[i].string);
        else
            ch++;

        len = mbstowcs(wc_name, ch, _countof(wc_name));
        wc_name[len] = '\0';

		if (_wcsicmp(wc_name, name) == 0)
        {
            node->id = i;
            return 0;
        }
    }

    return ENOTFOUND;
}

status_t RdLookupFile(fsd_t *fsd, vnode_id_t id, uint32_t open_flags, void **cookie)
{
    if (id >= kernel_startup.multiboot_info->mods_count)
        return ENOTFOUND;

    *cookie = (multiboot_module_t*) PHYSICAL(kernel_startup.multiboot_info->mods_addr) + id;
    return 0;
}

status_t RdGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
    const char *ch;
    dirent_all_t* di;
    size_t len;
    multiboot_module_t *mod;
    wchar_t *name;

    mod = cookie;
    di = buf;

    ch = strrchr(PHYSICAL(mod->string), '/');
    if (ch == NULL)
        ch = PHYSICAL(mod->string);
    else
        ch++;

    switch (type)
    {
    case FILE_QUERY_NONE:
        return 0;

    case FILE_QUERY_DIRENT:
        len = mbstowcs(di->dirent.name, ch, _countof(di->dirent.name));
        if (len == -1)
            wcscpy(di->dirent.name, L"?");
        else
            di->dirent.name[len] = '\0';

        di->dirent.vnode = mod 
            - (multiboot_module_t*) PHYSICAL(kernel_startup.multiboot_info->mods_addr);
        return 0;

    case FILE_QUERY_STANDARD:
        len = strlen(ch);
        name = malloc(sizeof(wchar_t) * (len + 1));
        if (name == NULL)
            return errno;

        len = mbstowcs(name, ch, len);
        if (len == -1)
            wcscpy(name, L"?");
        else
            name[len] = '\0';

        di->standard.length = mod->mod_end - mod->mod_start;
        di->standard.attributes = FILE_ATTR_READ_ONLY;
        FsGuessMimeType(wcsrchr(name, '.'), 
            di->standard.mimetype, 
            _countof(di->standard.mimetype) - 1);
        free(name);
        return 0;
    }

    return ENOTIMPL;
}

status_t RdReadWrite(fsd_t *fsd, const fs_request_t *req, size_t *bytes)
{
    multiboot_module_t *mod;
    void *ptr, *dest;
	size_t length;

    *bytes = 0;
	length = req->length;
    mod = req->file->fsd_cookie;
    if (req->pos + length >= mod->mod_end - mod->mod_start)
        length = mod->mod_end - mod->mod_start - req->pos;

    if (length == 0)
        return EEOF;

    ptr = (uint8_t*) PHYSICAL(mod->mod_start) + (uint32_t) req->pos;
    /*wprintf(L"RdRead: %x (%S) at %x => %x = %08x\n",
        mod->mod_start,
        PHYSICAL(mod->string),
        (uint32_t) file->pos, 
        (addr_t) ptr,
        *(uint32_t*) ptr);*/

    dest = MemMapPageArray(req->pages);
    memcpy(dest, ptr, length);
    MemUnmapPageArray(req->pages);
    FsNotifyCompletion(req->io, length, 0);
    return 0;
}

status_t RdOpenDir(fsd_t *fsd, vnode_id_t node, void **cookie)
{
    unsigned *index;
    assert(node == VNODE_ROOT);
    index = malloc(sizeof(unsigned));
    if (index == NULL)
        return errno;
    *index = 0;
    *cookie = index;
    return 0;
}

status_t RdReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
    unsigned *index;
    multiboot_module_t *first_mod, *mod;
    char *ch;
    size_t len;

    index = dir_cookie;
    first_mod = (multiboot_module_t*) PHYSICAL(kernel_startup.multiboot_info->mods_addr);
    if (*index < kernel_startup.multiboot_info->mods_count)
    {
        mod = first_mod + *index;
        KeAtomicInc(index);

        ch = strrchr(PHYSICAL(mod->string), '/');
        if (ch == NULL)
            ch = PHYSICAL(mod->string);
        else
            ch++;

        len = mbstowcs(buf->name, ch, _countof(buf->name));
        if (len == -1)
            wcscpy(buf->name, L"?");
        else
            buf->name[len] = '\0';

        buf->vnode = mod - first_mod;
        return 0;
    }
    else
        return EEOF;
}

void RdFreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
    free(dir_cookie);
}

static const struct vtbl_fsd_t ramdisk_vtbl =
{
    NULL,           /* dismount */
    NULL,           /* get_fs_info */
    RdParseElement,
    NULL,           /* create_file */
    RdLookupFile,
    RdGetFileInfo,
    NULL,           /* set_file_info */
    NULL,           /* free_cookie */
    RdReadWrite,
    NULL,           /* ioctl */
    NULL,           /* passthrough */
    NULL,           /* mkdir */
    RdOpenDir,
    RdReadDir,
    RdFreeDirCookie,
    //NULL,           /* mount */
    NULL,           /* flush_cache */
};

static fsd_t ramdisk_fsd =
{
    &ramdisk_vtbl,
};

fsd_t* RdMountFs(driver_t* driver, const wchar_t *dest)
{
    return &ramdisk_fsd;
}

driver_t rd_driver =
{
    NULL,
    NULL,
    NULL,
    NULL,
    RdMountFs
};

addr_t RdGetFilePhysicalAddress(const wchar_t *name, size_t *length)
{
    multiboot_module_t *mods;
    wchar_t wc_name[16];
    size_t len;
    char *ch;
    unsigned i;

    /*wprintf(L"RdLookupFile(%s)\n", path);*/
    mods = PHYSICAL(kernel_startup.multiboot_info->mods_addr);
    for (i = 0; i < kernel_startup.multiboot_info->mods_count; i++)
    {
        ch = strrchr(PHYSICAL(mods[i].string), '/');
        if (ch == NULL)
            ch = PHYSICAL(mods[i].string);
        else
            ch++;

        len = mbstowcs(wc_name, ch, _countof(wc_name));
        wc_name[len] = '\0';
        if (_wcsicmp(wc_name, name) == 0)
		{
			if (length != NULL)
				*length = mods[i].mod_end - mods[i].mod_start;

            return mods[i].mod_start;
		}
    }

    return NULL;
}
