/* $Id: ramdisk_mb.c,v 1.9 2002/05/05 13:43:24 pavlovskii Exp $ */

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

    assert(path[0] == '/');

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
        if (_wcsicmp(wc_name, path + 1) == 0)
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
    dirent_standard_t* di;
    size_t len;
    multiboot_module_t *mod;

    di = buf;
    mod = cookie;
    switch (type)
    {
    case FILE_QUERY_NONE:
        return 0;

    case FILE_QUERY_STANDARD:
        ch = strrchr(PHYSICAL(mod->string), '/');
        if (ch == NULL)
            ch = PHYSICAL(mod->string);
        else
            ch++;

        len = mbstowcs(di->di.name, ch, _countof(di->di.name));
        if (len == -1)
            wcscpy(di->di.name, L"?");
        else
            di->di.name[len] = '\0';

        di->di.vnode = mod 
            - (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
        di->length = mod->mod_end - mod->mod_start;
        di->attributes = FILE_ATTR_READ_ONLY;
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

#if 0
bool RdFsRequest(device_t* dev, request_t* req)
{
    request_fs_t *req_fs = (request_fs_t*) req;

    ramfd_t *fd;
    uint8_t *ptr;
    size_t len;
    dirent_t *buf;
    multiboot_module_t *mods, *mod;
    char *ch;

    mods = (multiboot_module_t*) kernel_startup.multiboot_info->mods_addr;
    switch (req->code)
    {
    case FS_OPEN:
        if (req_fs->params.fs_open.flags & FILE_WRITE)
        {
            req->result = EACCESS;
            return false;
        }

        mod = RdLookupFile(req_fs->params.fs_open.name);
        if (mod == NULL)
        {
            req_fs->header.result = ENOTFOUND;
            req_fs->params.fs_open.file = NULL;
            /*wprintf(L"%s: not found on ram disk\n", 
                req_fs->params.fs_open.name);*/
            return false;
        }

        req_fs->params.fs_open.file = HndAlloc(NULL, sizeof(ramfd_t), 'file');
        fd = HndLock(NULL, req_fs->params.fs_open.file, 'file');
        assert(fd != NULL);
        fd->file.fsd = dev;
        fd->file.pos = 0;
        fd->file.flags = req_fs->params.fs_open.flags;
        fd->mod = mod;
        HndUnlock(NULL, req_fs->params.fs_open.file, 'file');
        
        /*wprintf(L"RdFsRequest: FS_OPEN(%s), file = %p = %x, %d bytes\n", 
            name, 
            file, 
            *(uint32_t*) ((uint8_t*) ramdisk_header + fd->ram->offset),
            fd->ram->length);*/
        return true;

    case FS_OPENSEARCH:
        assert(req_fs->params.fs_opensearch.name[0] == '/');
        req_fs->params.fs_opensearch.file = HndAlloc(NULL, sizeof(ramfd_t), 'file');
        fd = HndLock(NULL, req_fs->params.fs_opensearch.file, 'file');
        if (fd == NULL)
        {
            req->result = errno;
            return false;
        }

        fd->file.fsd = dev;
        fd->file.pos = 0;
        fd->file.flags = FILE_READ;
        fd->mod = NULL;
        HndUnlock(NULL, req_fs->params.fs_opensearch.file, 'file');
        return true;

    case FS_QUERYFILE:
        mod = RdLookupFile(req_fs->params.fs_queryfile.name);
        if (mod == NULL)
        {
            req->result = ENOTFOUND;
            return false;
        }

        buf = req_fs->params.fs_queryfile.buffer;
        switch (req_fs->params.fs_queryfile.query_class)
        {
        case FILE_QUERY_NONE:
            break;

        case FILE_QUERY_STANDARD:
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

            buf->length = mod->mod_end - mod->mod_start;
            buf->standard_attributes = FILE_ATTR_READ_ONLY;
            break;
        }

        return true;

    case FS_CLOSE:
        HndClose(NULL, req_fs->params.fs_close.file, 'file');
        return true;

    case FS_READ:
        fd = HndLock(NULL, req_fs->params.fs_read.file, 'file');
        assert(fd != NULL);

        if (fd->mod != NULL)
        {
            /* Normal file */
            if (fd->file.pos + req_fs->params.fs_read.length >= 
                fd->mod->mod_end - fd->mod->mod_start)
                req_fs->params.fs_read.length = 
                    fd->mod->mod_end - fd->mod->mod_start - fd->file.pos;

            ptr = (uint8_t*) PHYSICAL(fd->mod->mod_start) + (uint32_t) fd->file.pos;
            /*wprintf(L"RdFsRequest: read %x (%S) at %x => %x = %08x\n",
                fd->mod->mod_start,
                PHYSICAL(fd->mod->string),
                (uint32_t) fd->file.pos, 
                (addr_t) ptr,
                *(uint32_t*) ptr);*/

            memcpy(MemMapPageArray(req_fs->params.fs_read.pages, 
                    PRIV_PRES | PRIV_KERN | PRIV_WR), 
                ptr,
                req_fs->params.fs_read.length);
            MemUnmapTemp();
            fd->file.pos += req_fs->params.fs_read.length;
        }
        else
        {
            /* Search */
            if (fd->file.pos >= kernel_startup.multiboot_info->mods_count)
            {
                req->result = EEOF;
                HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
                return false;
            }
            
            mod = mods + fd->file.pos;
            len = req_fs->params.fs_read.length;

            req_fs->params.fs_read.length = 0;
            buf = MemMapPageArray(req_fs->params.fs_read.pages, 
                PRIV_PRES | PRIV_KERN | PRIV_WR);
            while (req_fs->params.fs_read.length < len)
            {
                size_t temp;

                ch = strrchr(PHYSICAL(mod->string), '/');
                if (ch == NULL)
                    ch = PHYSICAL(mod->string);
                else
                    ch++;

                temp = mbstowcs(buf->name, ch, _countof(buf->name));
                if (temp == -1)
                    wcscpy(buf->name, L"?");
                else
                    buf->name[temp] = '\0';

                buf->length = mod->mod_end - mod->mod_start;
                buf->standard_attributes = FILE_ATTR_READ_ONLY;

                buf++;
                mod++;
                req_fs->params.fs_read.length += sizeof(dirent_t);

                fd->file.pos++;
                if (fd->file.pos >= kernel_startup.multiboot_info->mods_count)
                    break;
            }

            MemUnmapTemp();
        }

        HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
        return true;
    }

    req->result = ENOTIMPL;
    return false;
}
#endif

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
