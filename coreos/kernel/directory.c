/*
 * $History: directory.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 8/03/04    Time: 20:30
 * Updated in $/coreos/kernel
 * Moved FsCleanupDir into here
 * 
 * *****************  Version 1  *****************
 * User: Tim          Date: 7/03/04    Time: 19:05
 * Created in $/coreos/kernel
 */

#include <kernel/kernel.h>
#include <kernel/fs.h>

#include <kernel/debug.h>

#include <errno.h>

/*
 * Cleanup callback for file_dir_t objects
 */
static void FsCleanupDir(void *ptr)
{
    file_dir_t *dh;
    dh = ptr;
    if (dh->fsd != NULL &&
        dh->fsd->vtbl->free_dir_cookie != NULL)
        dh->fsd->vtbl->free_dir_cookie(dh->fsd, dh->fsd_cookie);
    free(dh);
}


bool FsCreateDir(const wchar_t *path)
{
    status_t ret;
    void *dir_cookie;
    vnode_t node;
    const wchar_t *name;

    ret = FsPathToVnode(&fs_root, path, &node, true, &name);
    if (ret == 0)
    {
        if (name == NULL)
            ret = EINVALID;
        else if (node.fsd->vtbl->mkdir == NULL)
            ret = ENOTIMPL;
        else
            ret = node.fsd->vtbl->mkdir(node.fsd, node.id, name, &dir_cookie);
    }

    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    if (node.fsd->vtbl->free_dir_cookie != NULL)
        node.fsd->vtbl->free_dir_cookie(node.fsd, dir_cookie);

    return true;
}


/*!
 *    \brief    Opens a directory search
 *
 *    \param    path    Full search specification
 *    \return    Handle to the search
 */
file_dir_t *FsOpenDir(const wchar_t *path)
{
    static handle_class_t directory_class =
        HANDLE_CLASS('fdir', FsCleanupDir, NULL, NULL, NULL);
    status_t ret;
    void *dir_cookie;
    file_dir_t *dh;
    vnode_t node;

    dh = malloc(sizeof(file_dir_t));
    if (dh == NULL)
        return 0;

    HndInit(&dh->hdr, &directory_class);
    dh->fsd = NULL;
    dh->fsd_cookie = NULL;

    ret = FsPathToVnode(&fs_root, path, &node, false, NULL);
    if (ret == 0)
    {
        /*
         * node could contain the fsd:id of a mount point. We want to open 
         *  the root of the mounted FS, not the location where it is mounted.
         */
        FsLookupMountPoint(&node, &node);
        if (node.fsd->vtbl->opendir == NULL)
            ret = ENOTIMPL;
        else
            ret = node.fsd->vtbl->opendir(node.fsd, node.id, &dir_cookie);
    }

    if (ret != 0)
    {
        errno = ret;
        HndClose(&dh->hdr);
        return NULL;
    }

    TRACE3("FsOpenDir(%s): (%p/%x)\n", path, node.fsd, node.id);

    dh->fsd = node.fsd;
    dh->fsd_cookie = dir_cookie;
    return dh;
}


bool FsReadDir(file_dir_t *dh, dirent_t *di, size_t size)
{
    fsd_t *fsd;
    status_t ret;

    if (!MemVerifyBuffer(di, size) ||
        size < sizeof(dirent_t))
    {
        errno = EBUFFER;
        return false;
    }

    fsd = dh->fsd;

    if (fsd->vtbl->readdir == NULL)
        ret = errno = ENOTIMPL;
    else
        ret = fsd->vtbl->readdir(fsd, dh->fsd_cookie, di);

    return ret > 0 ? false : true;
}
