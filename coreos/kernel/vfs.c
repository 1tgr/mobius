/*
 * $History: vfs.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 7/03/04    Time: 13:52
 * Updated in $/coreos/kernel
 * Put directory names inside the vfs_dir_t structure, instead of making a
 * separate allocation
 * Removed memory leaks on unmount
 * Added history block
 */

#include <kernel/fs.h>

#include <errno.h>
#include <wchar.h>
#include <string.h>

typedef struct vfs_dir_t vfs_dir_t;
struct vfs_dir_t
{
    vfs_dir_t *prev, *next, *parent;
    vfs_dir_t *child_first, *child_last;
    wchar_t name[1];
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
    vfs_dir_t *dir;
};


static void VfsRemoveDir(vfs_t *vfs, vfs_dir_t *dir)
{
    vfs_dir_t *child, *next;

    for (child = dir->child_first; child != NULL; child = next)
    {
        next = child->next;
        VfsRemoveDir(vfs, child);
    }

    if (dir != &vfs->root)
        free(dir);
}


static void VfsDismount(fsd_t *fsd)
{
    vfs_t *vfs;
    vfs = (vfs_t*) fsd;
    VfsRemoveDir(vfs, &vfs->root);
    free(fsd);
}


static void VfsGetFsInfo(fsd_t *fsd, fs_info_t *info)
{
    if (info->flags & FS_INFO_CACHE_BLOCK_SIZE)
        info->cache_block_size = 0;
}


static status_t VfsParseElement(fsd_t *fsd, const wchar_t *name, wchar_t **new_path, vnode_t *node)
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


static status_t VfsCreateFile(fsd_t *fsd, vnode_id_t dir, const wchar_t *name, void **cookie)
{
    return EACCESS;
}


static status_t VfsLookupFile(fsd_t *fsd, vnode_id_t node, uint32_t open_flags, void **cookie)
{
    vfs_t *vfs;

    vfs = (vfs_t*) fsd;
    if (node == VNODE_ROOT)
        *cookie = &vfs->root;
    else
        *cookie = (vfs_dir_t*) node;

    return 0;
}


static status_t VfsGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
    vfs_dir_t *dir;
    dirent_all_t *di;

    di = buf;
    dir = cookie;
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


static status_t VfsSetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, const void *buf)
{
    return EACCESS;
}


static void VfsFreeCookie(fsd_t *fsd, void *cookie)
{
}


static status_t VfsMkDir(fsd_t *fsd, vnode_id_t dir, const wchar_t *name, void **dir_cookie)
{
    vfs_t *vfs;
    vfs_dir_t *parent, *new;

    vfs = (vfs_t*) fsd;
    if (dir == VNODE_ROOT)
        parent = &vfs->root;
    else
        parent = (vfs_dir_t*) dir;

    new = malloc(sizeof(vfs_dir_t) + wcslen(name) * sizeof(wchar_t));
    if (new == NULL)
        return errno;

    new->child_first = new->child_last = NULL;
    new->parent = parent;
    wcscpy(new->name, name);
    LIST_ADD(parent->child, new);
    *dir_cookie = NULL;
    return 0;
}


static status_t VfsOpenDir(fsd_t *fsd, vnode_id_t node, void **dir_cookie)
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
}


static status_t VfsReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
    vfs_search_t *search;
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


static void VfsFreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
    vfs_search_t *search;
    search = dir_cookie;
    free(search);
}


static const struct vtbl_fsd_t vfs_vtbl =
{
    VfsDismount,
    VfsGetFsInfo,
    VfsParseElement,
    VfsCreateFile,
    VfsLookupFile,
    VfsGetFileInfo,
    VfsSetFileInfo,
    VfsFreeCookie,
    NULL,           /* read_write */
    NULL,           /* ioctl */
    NULL,           /* passthrough */
    VfsMkDir,
    VfsOpenDir,
    VfsReadDir,
    VfsFreeDirCookie,
    NULL,           /* finishio */
    NULL,           /* flush_cache */
};


static fsd_t *VfsMountFs(driver_t *drv, const wchar_t *dest)
{
    vfs_t *vfs;

    vfs = malloc(sizeof(vfs_t));
    if (vfs == NULL)
        return NULL;

    memset(vfs, 0, sizeof(vfs_t));
    vfs->fsd.vtbl = &vfs_vtbl;
    vfs->root.parent = &vfs->root;
    vfs->root.name[0] = '\0';

    return &vfs->fsd;
}


extern struct module_t mod_kernel;

driver_t vfs_driver = 
{
    &mod_kernel,
    NULL,
    NULL,
    NULL,
    VfsMountFs,
};
