/* $Id: ramdisk_mb.c,v 1.11 2002/06/09 18:43:05 pavlovskii Exp $ */

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

typedef struct ramfd_t ramfd_t;
struct ramfd_t
{
    file_t file;
    multiboot_module_t *mod;
};

status_t RdLookupFile(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **cookie)
{
    multiboot_module_t *mods;
    wchar_t wc_name[16];
    size_t len;
    char *ch;
    unsigned i;

    if (path[0] == '/')
        path++;

    /*wprintf(L"RdLookupFile(%s)\n", path);*/
    mods = (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
    for (i = 0; i < kernel_startup.multiboot_info->mods_count; i++)
    {
        ch = strrchr(PHYSICAL(mods[i].string), '/');
        if (ch == NULL)
            ch = PHYSICAL(mods[i].string);
        else
            ch++;

        len = mbstowcs(wc_name, ch, _countof(wc_name));
        wc_name[len] = '\0';
        if (_wcsicmp(wc_name, path) == 0)
        {
            *cookie = mods + i;
            return 0;
        }
    }

    return ENOTFOUND;
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
            - (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
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

bool RdRead(fsd_t *fsd, file_t *file, page_array_t *pages, size_t length, fs_asyncio_t *io)
{
    multiboot_module_t *mod;
    void *ptr;

    mod = file->fsd_cookie;
    if (file->pos + length >= mod->mod_end - mod->mod_start)
        length = mod->mod_end - mod->mod_start - file->pos;

    ptr = (uint8_t*) PHYSICAL(mod->mod_start) + (uint32_t) file->pos;
    /*wprintf(L"RdRead: %x (%S) at %x => %x = %08x\n",
        mod->mod_start,
        PHYSICAL(mod->string),
        (uint32_t) file->pos, 
        (addr_t) ptr,
        *(uint32_t*) ptr);*/

    memcpy(MemMapPageArray(pages, PRIV_PRES | PRIV_KERN | PRIV_WR), 
        ptr,
        length);
    MemUnmapTemp();
    FsNotifyCompletion(io, length, 0);
    return true;
}

status_t RdOpenDir(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **cookie)
{
    unsigned *index;
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
    first_mod = (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
    if (*index < kernel_startup.multiboot_info->mods_count)
    {
        mod = first_mod + *index;
        (*index)++;

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

/*!    \brief Initializes the ramdisk during kernel startup.
 *
 *    This routine is called by \p KernelMain to check the ramdisk (loaded by the
 *          second-stage boot routine) and map it into the kernel's address
 *          space. Because it is mapped into the kernel's address space,
 *          it will be mapped into subsequent address spaces as needed.
 *
 *    \note    All objects in the ramdisk (including the ramdisk header, file
 *          headers and the file data themselves) should be aligned on page 
 *          boundaries. This is done automatically by the boot loader.
 *
 *    \return         \p true if the ramdisk was correct
 */

descriptor_t arch_gdt[11];

bool RdInit(void)
{
    /*unsigned i;
    multiboot_module_t *mods;*/
    /*wprintf(L"ramdisk: multiboot_info at %p, DS base = %x\n", 
        kernel_startup.multiboot_info, 
        arch_gdt[4].base_l | (arch_gdt[4].base_m << 16) | (arch_gdt[4].base_h << 24));
    wprintf(L"ramdisk: number of modules = %u\n",
        kernel_startup.multiboot_info->mods_count);*/
    /*mods = (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
    for (i = 0; i < kernel_startup.multiboot_info->mods_count; i++)
        wprintf(L"module %u: %S: %x=>%x\n", i, PHYSICAL(mods[i].string), 
            mods[i].mod_start, mods[i].mod_end);*/
    /*halt(0);*/
    return true;
}

static const fsd_vtbl_t ramdisk_vtbl =
{
    NULL,           /* dismount */
    NULL,           /* get_fs_info */
    NULL,           /* create_file */
    RdLookupFile,
    RdGetFileInfo,
    NULL,           /* set_file_info */
    NULL,           /* free_cookie */
    RdRead,
    NULL,           /* write */
    NULL,           /* ioctl */
    NULL,           /* passthrough */
    RdOpenDir,
    RdReadDir,
    RdFreeDirCookie,
    NULL,           /* mount */
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

addr_t RdGetFilePhysicalAddress(const wchar_t *name)
{
    multiboot_module_t *mods;
    wchar_t wc_name[16];
    size_t len;
    char *ch;
    unsigned i;

    /*wprintf(L"RdLookupFile(%s)\n", path);*/
    mods = (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
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
            return mods[i].mod_start;
    }

    return NULL;
}
