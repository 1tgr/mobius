/* $Id: vfs.c,v 1.1 2002/05/05 13:44:45 pavlovskii Exp $ */

#include <kernel/fs.h>

#include <errno.h>
#include <wchar.h>

typedef struct vfs_mount_t vfs_mount_t;
struct vfs_mount_t
{
    vfs_mount_t *prev, *next;
    wchar_t *name;
    fsd_t *fsd;
};

typedef struct vfs_dir_t vfs_dir_t;
struct vfs_dir_t
{
    fsd_t fsd;
    vfs_mount_t *vfs_mount_first, *vfs_mount_last;
};

typedef struct vfs_search_t vfs_search_t;
struct vfs_search_t
{
    vfs_mount_t *mount;
};

vfs_mount_t *VfsLookupMount(vfs_dir_t *dir, const wchar_t **path, bool *do_redirect)
{
    vfs_mount_t *mount;
    wchar_t *ch;
    const wchar_t *p;
    size_t len;

    p = *path;
    if (p[0] == '/')
        p++;

    ch = wcschr(p, '/');
    if (ch == NULL)
    {
        *do_redirect = false;
        len = wcslen(p);
    }
    else
    {
        *do_redirect = true;
        len = ch - p;
    }

    FOREACH (mount, dir->vfs_mount)
        if (_wcsnicmp(mount->name, p, len) == 0)
        {
            *path = p + len;
            return mount;
        }

    wprintf(L"%s not found: Path = %s Name = %s Len = %d\n", *path, p, ch, len);
    return NULL;
}

void VfsDismount(fsd_t *fsd)
{
}

void VfsGetFsInfo(fsd_t *fsd, fs_info_t *info)
{
    info->cache_block_size = 0;
}

status_t VfsCreateFile(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **cookie)
{
    vfs_mount_t *mount;
    bool do_redirect;

    mount = VfsLookupMount((vfs_dir_t*) fsd, &path, &do_redirect);
    if (mount == NULL)
        return ENOTFOUND;
    else
    {
        if (do_redirect)
        {
            *redirect = mount->fsd;
            if (mount->fsd->vtbl->create_file == NULL)
                return ENOTIMPL;
            else
                return mount->fsd->vtbl->create_file(mount->fsd, path, redirect, cookie);
        }
        else
            return EACCESS;
    }
}

status_t VfsLookupFile(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **cookie)
{
    vfs_mount_t *mount;
    bool do_redirect;

    //wprintf(L"VfsLookupFile(%s): ", path);
    mount = VfsLookupMount((vfs_dir_t*) fsd, &path, &do_redirect);
    //wprintf(L"do_redirect = %u, mount = %p, name = %s\n", do_redirect, mount, path);
    if (mount == NULL)
        return ENOTFOUND;
    else
    {
        if (do_redirect)
        {
            *redirect = mount->fsd;
            /*wprintf(L"VfsLookupFile(%s): fsd = %p vtbl = %p\n", 
                path, mount->fsd, mount->fsd->vtbl);*/
            if (mount->fsd->vtbl->lookup_file == NULL)
                return ENOTIMPL;
            else
                return mount->fsd->vtbl->lookup_file(mount->fsd, path, redirect, cookie);
        }
        else
        {
            *cookie = mount;
            *redirect = fsd;
            return 0;
        }
    }
}

status_t VfsGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
    vfs_mount_t *mount;
    dirent_standard_t *di;

    mount = cookie;
    /*wprintf(L"VfsGetFileInfo(%p) = %s\n", cookie, mount->name);*/
    di = buf;
    switch (type)
    {
    case FILE_QUERY_NONE:
        return 0;

    case FILE_QUERY_STANDARD:
        wcsncpy(di->di.name, mount->name, _countof(di->di.name) - 1);
        di->di.vnode = 0;
        di->length = 0;
        di->attributes = FILE_ATTR_DIRECTORY;
        return 0;
    }

    return ENOTIMPL;
}

status_t VfsSetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, const void *buf)
{
    return EACCESS;
}

void VfsFreeCookie(fsd_t *fsd, void *cookie)
{
}

/*bool VfsRead(fsd_t *fsd, file_t *file, page_array_t *pages, 
             size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}

bool VfsWrite(fsd_t *fsd, file_t *file, page_array_t *pages, 
              size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}

bool VfsIoCtl(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
              size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}

bool VfsPassthrough(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
                    size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
}*/

status_t VfsOpenDir(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **dir_cookie)
{
    vfs_mount_t *mount;
    vfs_dir_t *dir;
    wchar_t *ch;
    size_t len;
    bool do_redirect;
    vfs_search_t *search;

    dir = (vfs_dir_t*) fsd;
    if (path[0] == '/')
        path++;

    if (*path == '\0')
    {
        search = malloc(sizeof(vfs_search_t));
        if (search == NULL)
            return errno;

        search->mount = dir->vfs_mount_first;
        *dir_cookie = search;
        *redirect = fsd;
        return 0;
    }

    ch = wcschr(path, '/');
    if (ch == NULL)
    {
        do_redirect = false;
        len = wcslen(path);
    }
    else
    {
        do_redirect = true;
        len = ch - path;
    }

    FOREACH (mount, dir->vfs_mount)
        if (_wcsnicmp(mount->name, path, len) == 0)
        {
            path += len;
            *redirect = mount->fsd;
            if (mount->fsd->vtbl->opendir == NULL)
                return ENOTIMPL;
            else
                return mount->fsd->vtbl->opendir(mount->fsd, path, redirect, dir_cookie);
        }

    return ENOTFOUND;
}

status_t VfsReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
    vfs_search_t *search;
    vfs_mount_t *mount;

    search = dir_cookie;
    mount = search->mount;
    if (mount == NULL)
        return EEOF;
    else
    {
        wcsncpy(buf->name, mount->name, _countof(buf->name) - 1);
        buf->vnode = 0;
        search->mount = mount->next;
        return 0;
    }
}

void VfsFreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
    vfs_search_t *search;
    search = dir_cookie;
    free(search);
}

status_t VfsMount(fsd_t *fsd, const wchar_t *path, fsd_t *newfsd)
{
    const wchar_t *ch;
    size_t len;
    vfs_mount_t *mount;
    vfs_dir_t *dir;

    dir = (vfs_dir_t*) fsd;
    /*wprintf(L"VfsMount(%s, %p)\n", path, newfsd);*/
    if (path[0] == '/')
        path++;

    ch = wcschr(path, '/');
    if (ch == NULL)
    {
        len = wcslen(path);
        ch = path + len;
    }
    else
        len = ch - path;

    if (*ch == '\0')
    {
        /* Mounting on this directory */
        /*wprintf(L"VfsMount: mounting %p as %*s\n",
            newfsd, len, path);*/

        mount = malloc(sizeof(vfs_mount_t));
        mount->name = malloc(sizeof(wchar_t) * (len + 1));
        mount->name[len] = '\0';
        memcpy(mount->name, path, sizeof(wchar_t) * len);
        mount->fsd = newfsd;
        LIST_ADD(dir->vfs_mount, mount);

        return 0;
    }
    else
    {
        /* Mounting on a subdirectory */
        /*wprintf(L"VfsMount: mounting on subdirectory %s\n", path);*/

        FOREACH (mount, dir->vfs_mount)
            if (_wcsnicmp(mount->name, path, len) == 0)
            {
                /*TRACE1("=> %p\n", mount->fsd);*/
                path += len;
                if (mount->fsd->vtbl->mount == NULL)
                    return ENOTIMPL;
                else
                    return mount->fsd->vtbl->mount(mount->fsd, ch, newfsd);
            }

        return ENOTFOUND;
    }
}

static const fsd_vtbl_t vfs_vtbl =
{
    VfsDismount,
    VfsGetFsInfo,
    VfsCreateFile,
    VfsLookupFile,
    VfsGetFileInfo,
    VfsSetFileInfo,
    VfsFreeCookie,
    NULL,           /* read */
    NULL,           /* write */
    NULL,           /* ioctl */
    NULL,           /* passthrough */
    VfsOpenDir,
    VfsReadDir,
    VfsFreeDirCookie,
    VfsMount,
    NULL,           /* finishio */
    NULL,           /* flush_cache */
};

#if 0
static bool VfsRequest(device_t *dev, request_t *req)
{
    vfs_dir_t *dir = (vfs_dir_t*) dev;
    request_fs_t *req_fs = (request_fs_t*) req;
    const wchar_t *ch;
    size_t len;
    vfs_mount_t *mount;
    vfs_search_t *search;
    dirent_t *buf;

    switch (req->code)
    {
    /* Assumes params_fs_t.fs_create <=> params_fs_t.fs_open */
    case FS_CREATE:
    case FS_OPEN:
        if (req_fs->params.fs_open.name[0] == '/')
            req_fs->params.fs_open.name++;

        ch = wcschr(req_fs->params.fs_open.name, '/');
        if (ch == NULL)
            len = wcslen(req_fs->params.fs_open.name);
        else
            len = ch - req_fs->params.fs_open.name;

        /*TRACE3("Path: %s Name: %s Len: %d\n", 
            req_fs->params.fs_open.name, ch, len);*/

        FOREACH (mount, dir->vfs_mount)
            if (_wcsnicmp(mount->name, req_fs->params.fs_open.name, len) == 0)
            {
                /*TRACE1("=> %p\n", mount->fsd);*/
                req_fs->params.fs_open.name += len;
                return mount->fsd->vtbl->request(mount->fsd, req);
            }
        
        wprintf(L"%s: not found in root\n", req_fs->params.fs_open.name);
        req->result = ENOTFOUND;
        return false;
        
    case FS_MOUNT:
        if (req_fs->params.fs_mount.name[0] == '/')
            req_fs->params.fs_mount.name++;

        ch = wcschr(req_fs->params.fs_mount.name, '/');
        if (ch == NULL)
        {
            len = wcslen(req_fs->params.fs_mount.name);
            ch = req_fs->params.fs_mount.name + len;
        }
        else
            len = ch - req_fs->params.fs_mount.name;
        
        if (*ch == '\0')
        {
            /* Mounting on this directory */
            /*TRACE3("VfsRequest(FS_MOUNT): mounting %p as %*s\n",
                req_fs->params.fs_mount.fsd, 
                len,
                req_fs->params.fs_mount.name);*/

            mount = malloc(sizeof(vfs_mount_t));
            mount->name = malloc(sizeof(wchar_t) * (len + 1));
            mount->name[len] = '\0';
            memcpy(mount->name, req_fs->params.fs_mount.name, 
                sizeof(wchar_t) * len);
            mount->fsd = req_fs->params.fs_mount.fsd;
            LIST_ADD(dir->vfs_mount, mount);

            return true;
        }
        else
        {
            /* Mounting on a subdirectory */
            /*TRACE1("VfsRequest(FS_MOUNT): mounting on subdirectory %s\n", ch);*/

            FOREACH (mount, dir->vfs_mount)
                if (_wcsnicmp(mount->name, req_fs->params.fs_open.name, len) == 0)
                {
                    /*TRACE1("=> %p\n", mount->fsd);*/
                    req_fs->params.fs_mount.name += len;
                    return mount->fsd->vtbl->request(mount->fsd, req);
                }

            req->result = ENOTFOUND;
            return false;
        }

    case FS_OPENSEARCH:
        if (req_fs->params.fs_opensearch.name[0] == '/')
            req_fs->params.fs_opensearch.name++;

        ch = wcschr(req_fs->params.fs_opensearch.name, '/');
        if (ch == NULL)
            len = wcslen(req_fs->params.fs_opensearch.name);
        else
            len = ch - req_fs->params.fs_opensearch.name;

        TRACE3("Search: Path: %s Name: %s Len: %d\n", 
            req_fs->params.fs_open.name, ch, len);

        if (ch != NULL)
        {
            FOREACH (mount, dir->vfs_mount)
                if (_wcsnicmp(mount->name, req_fs->params.fs_opensearch.name, len) == 0)
                {
                    /*TRACE1("=> %p\n", mount->fsd);*/
                    req_fs->params.fs_opensearch.name += len;
                    return mount->fsd->vtbl->request(mount->fsd, req);
                }

            req->result = ENOTFOUND;
            return false;
        }
        else
        {        
            /* It's a search in our directory */
            req_fs->params.fs_opensearch.file = HndAlloc(NULL, sizeof(vfs_search_t), 'file');
            search = HndLock(NULL, req_fs->params.fs_opensearch.file, 'file');
            if (search == NULL)
            {
                req->result = errno;
                return false;
            }

            search->fsd = dev;
            search->pos = (uint64_t) (addr_t) dir->vfs_mount_first;
            search->flags = FILE_READ;
            return true;
        }

    case FS_CLOSE:
        if (HndClose(NULL, req_fs->params.fs_close.file, 'file'))
            return true;
        else
        {
            req->result = errno;
            return false;
        }

    case FS_READ:
        search = HndLock(NULL, req_fs->params.fs_read.file, 'file');
        if (search == NULL)
        {
            req->result = errno;
            HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
            return false;
        }

        mount = (vfs_mount_t*) (addr_t) search->pos;
        if (mount == NULL)
        {
            req->result = EEOF;
            req_fs->params.fs_read.length = 0;
            HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
            return false;
        }

        buf = MemMapPageArray(req_fs->params.fs_read.pages, 
            PRIV_KERN | PRIV_PRES | PRIV_WR);
        len = req_fs->params.fs_read.length;
        req_fs->params.fs_read.length = 0;
        while (req_fs->params.fs_read.length < len)
        {
            wcscpy(buf->name, mount->name);
            buf->length = 0;
            buf->standard_attributes = FILE_ATTR_DIRECTORY;

            buf++;
            req_fs->params.fs_read.length += sizeof(dirent_t);

            mount = mount->next;
            search->pos = (uint64_t) (addr_t) mount;

            if (mount == NULL)
                break;
        }

        MemUnmapTemp();
        HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
        return true;
        
    case FS_QUERYFILE:
        if (req_fs->params.fs_queryfile.name[0] == '/')
            req_fs->params.fs_queryfile.name++;

        ch = wcschr(req_fs->params.fs_queryfile.name, '/');
        if (ch == NULL)
            len = wcslen(req_fs->params.fs_queryfile.name);
        else
            len = ch - req_fs->params.fs_queryfile.name;

        FOREACH (mount, dir->vfs_mount)
            if (_wcsnicmp(mount->name, req_fs->params.fs_queryfile.name, len) == 0)
            {
                req_fs->params.fs_queryfile.name += len;
                if (*req_fs->params.fs_queryfile.name == '\0' ||
                    wcscmp(req_fs->params.fs_queryfile.name, L"/") == 0)
                {
                    switch (req_fs->params.fs_queryfile.query_class)
                    {
                    case FILE_QUERY_NONE:
                        return true;

                    case FILE_QUERY_STANDARD:
                        if (req_fs->params.fs_queryfile.buffer_size < sizeof(dirent_t))
                        {
                            req->result = EBUFFER;
                            return false;
                        }

                        buf = (dirent_t*) req_fs->params.fs_queryfile.buffer;
                        wcscpy(buf->name, mount->name);
                        buf->length = 0;
                        buf->standard_attributes = FILE_ATTR_DIRECTORY;
                        return true;

                    default:
                        req->result = ENOTIMPL;
                        return false;
                    }
                }
                else
                    return mount->fsd->vtbl->request(mount->fsd, req);
            }

        wprintf(L"%s: not found in root\n", req_fs->params.fs_queryfile.name);
        req->result = ENOTFOUND;
        return false;
    }

    req->result = ENOTIMPL;
    return false;
}

static void VfsFinishIo(device_t *dev, request_t *req)
{
    fileop_t *op;
    request_fs_t *req_fs;

    /*if (req->code == FS_READ)
        TRACE3("VfsFinishIo: req = %p op = %p code = %x: ", 
            req, req->param, req->code);*/

    assert(req->code == FS_READ || req->code == FS_WRITE);
    op = req->param;
    op->result = req->result;
    req_fs = (request_fs_t*) req;
    if (req->code == FS_READ || req->code == FS_WRITE)
        op->bytes = req_fs->params.buffered.length;

    /*if (req->code == FS_READ)
        TRACE3("finished io: event = %u bytes = %u result = %u\n",
            op->event, op->bytes, op->result);*/

    EvtSignal(NULL, op->event);
    free(req);
}
#endif

bool FsMountDevice(const wchar_t *path, fsd_t *newfsd);

/*!
 *    \brief    Creates a new virtual directory
 *
 *    This effectively creates a new virtual file system and mounts it at
 *    \p path.
 *    \param    path    Full path specification for the new directory
 *    \return    \p true if the directory was created
 */
bool FsCreateVirtualDir(const wchar_t *path)
{
    vfs_dir_t *dir;

    dir = malloc(sizeof(vfs_dir_t));
    if (dir == NULL)
        return NULL;

    memset(dir, 0, sizeof(vfs_dir_t));
    dir->fsd.vtbl = &vfs_vtbl;

    if (!FsMountDevice(path, &dir->fsd))
    {
        free(dir);
        return false;
    }

    return true;
}
