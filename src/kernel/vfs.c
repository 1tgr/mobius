/* $Id: vfs.c,v 1.4 2002/08/14 16:24:00 pavlovskii Exp $ */

#include <kernel/fs.h>

#include <errno.h>
#include <wchar.h>

/*typedef struct vfs_mount_t vfs_mount_t;
struct vfs_mount_t
{
    vfs_mount_t *prev, *next;
    wchar_t *name;
    fsd_t *fsd;
};*/

typedef struct vfs_dir_t vfs_dir_t;
struct vfs_dir_t
{
    //vfs_mount_t *vfs_mount_first, *vfs_mount_last;
    vfs_dir_t *prev, *next, *parent;
    const wchar_t *name;
    vfs_dir_t *child_first, *child_last;
};

typedef struct vfs_t vfs_t;
struct vfs_t
{
    fsd_t fsd;
    vfs_dir_t root;
};

typedef struct vfs_search_t vfs_search_t;
struct vfs_search_t
{
    //vfs_mount_t *mount;
    vfs_dir_t *dir;
};

extern struct module_t mod_kernel;

/*static vfs_mount_t *VfsLookupMount(vfs_dir_t *dir, const wchar_t **path, bool *do_redirect)
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

    return NULL;
}*/

void VfsDismount(fsd_t *fsd)
{
    free(fsd);
}

void VfsGetFsInfo(fsd_t *fsd, fs_info_t *info)
{
    if (info->flags & FS_INFO_CACHE_BLOCK_SIZE)
        info->cache_block_size = 0;
}

status_t VfsParseElement(fsd_t *fsd, const wchar_t *name, wchar_t **new_path, vnode_t *node)
{
    vfs_t *vfs;
    vfs_dir_t *dir;

    vfs = (vfs_t*) fsd;
    if (node->id == VNODE_ROOT)
        dir = &vfs->root;
    else
        dir = (vfs_dir_t*) node->id;

    if (wcscmp(name, L"..") == 0)
    {
        node->id = (vnode_id_t) dir->parent;
        return 0;
    }
    else
        for (dir = dir->child_first; dir != NULL; dir = dir->next)
            if (_wcsicmp(dir->name, name) == 0)
            {
                node->id = (vnode_id_t) dir;
                return 0;
            }

    return ENOTFOUND;
}

status_t VfsCreateFile(fsd_t *fsd, vnode_id_t dir, const wchar_t *name, void **cookie)
{
    /*vfs_mount_t *mount;
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
    }*/

    return EACCESS;
}

status_t VfsLookupFile(fsd_t *fsd, vnode_id_t node, void **cookie)
{
    /*vfs_mount_t *mount;
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
            //wprintf(L"VfsLookupFile(%s): fsd = %p vtbl = %p\n", 
                //path, mount->fsd, mount->fsd->vtbl);
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
    }*/

    vfs_t *vfs;

    vfs = (vfs_t*) fsd;
    if (node == VNODE_ROOT)
        *cookie = &vfs->root;
    else
        *cookie = (vfs_dir_t*) node;

    return 0;
}

status_t VfsGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
    //vfs_mount_t *mount;
    vfs_dir_t *dir;
    dirent_all_t *di;

    di = buf;
    //mount = cookie;
    dir = cookie;
    /*wprintf(L"VfsGetFileInfo(%p) = %s\n", cookie, mount->name);*/
    di = buf;
    switch (type)
    {
    case FILE_QUERY_NONE:
        return 0;

    case FILE_QUERY_DIRENT:
        wcsncpy(di->dirent.name, dir->name, _countof(di->dirent.name) - 1);
        di->dirent.vnode = 0;
        return 0;

    case FILE_QUERY_STANDARD:
        di->standard.length = 0;
        di->standard.attributes = FILE_ATTR_DIRECTORY;
        wcscpy(di->standard.mimetype, L"");
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

status_t VfsMkDir(fsd_t *fsd, vnode_id_t dir, const wchar_t *name, void **dir_cookie)
{
    vfs_t *vfs;
    vfs_dir_t *parent, *new;

    vfs = (vfs_t*) fsd;
    if (dir == VNODE_ROOT)
        parent = &vfs->root;
    else
        parent = (vfs_dir_t*) dir;

    new = malloc(sizeof(vfs_dir_t));
    if (new == NULL)
        return errno;

    new->child_first = new->child_last = NULL;
    new->name = _wcsdup(name);
    new->parent = parent;
    LIST_ADD(parent->child, new);
    *dir_cookie = NULL;
    return 0;
}

status_t VfsOpenDir(fsd_t *fsd, vnode_id_t node, void **dir_cookie)
{
    vfs_t *vfs;
    vfs_dir_t *dir;
    vfs_search_t *search;

    vfs = (vfs_t*) fsd;
    if (node == VNODE_ROOT)
        dir = &vfs->root;
    else
        dir = (vfs_dir_t*) node;

    search = malloc(sizeof(vfs_search_t));
    if (search == NULL)
        return errno;

    search->dir = dir->child_first;
    *dir_cookie = search;
    return 0;

    /*vfs_mount_t *mount;
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

    return ENOTFOUND;*/
}

status_t VfsReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
    vfs_search_t *search;
    //vfs_mount_t *mount;
    vfs_dir_t *dir;

    search = dir_cookie;
    if (search == NULL)
        return EEOF;

    dir = search->dir;
    if (dir == NULL)
        return EEOF;
    else
    {
        wcsncpy(buf->name, dir->name, _countof(buf->name) - 1);
        buf->vnode = 0;
        search->dir = dir->next;
        return 0;
    }
}

void VfsFreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
    vfs_search_t *search;
    search = dir_cookie;
    free(search);
}

#if 0
status_t VfsMount(fsd_t *fsd, vnode_id_t node, const wchar_t *name, fsd_t *newfsd)
{
    vfs_dir_t *dir;
    vfs_mount_t *mount;

    assert(node == VNODE_ROOT);
    dir = (vfs_dir_t*) fsd;

    mount = malloc(sizeof(vfs_mount_t));
    mount->name = _wcsdup(name);
    mount->fsd = newfsd;
    LIST_ADD(dir->vfs_mount, mount);
    return 0;

#if 0
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
#endif
}
#endif

static const fsd_vtbl_t vfs_vtbl =
{
    VfsDismount,
    VfsGetFsInfo,
    VfsParseElement,
    VfsCreateFile,
    VfsLookupFile,
    VfsGetFileInfo,
    VfsSetFileInfo,
    VfsFreeCookie,
    NULL,           /* read */
    NULL,           /* write */
    NULL,           /* ioctl */
    NULL,           /* passthrough */
    VfsMkDir,
    VfsOpenDir,
    VfsReadDir,
    VfsFreeDirCookie,
    //VfsMount,
    NULL,           /* finishio */
    NULL,           /* flush_cache */
};

//bool FsMountDevice(const wchar_t *path, fsd_t *newfsd);

/*!
 *    \brief    Creates a new virtual directory
 *
 *    This effectively creates a new virtual file system and mounts it at
 *    \p path.
 *    \param    path    Full path specification for the new directory
 *    \return    \p true if the directory was created
 */
/*bool FsCreateVirtualDir(const wchar_t *path)
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
}*/

fsd_t *VfsMountFs(driver_t *drv, const wchar_t *dest)
{
    vfs_t *vfs;

    vfs = malloc(sizeof(vfs_t));
    if (vfs == NULL)
        return NULL;

    memset(vfs, 0, sizeof(vfs_t));
    vfs->fsd.vtbl = &vfs_vtbl;
    vfs->root.name = _wcsdup(L"/");
    vfs->root.parent = &vfs->root;

    return &vfs->fsd;
}

driver_t vfs_driver = 
{
    &mod_kernel,
    NULL,
    NULL,
    NULL,
    VfsMountFs,
};
