/* $Id: fs.c,v 1.29 2002/08/17 19:13:32 pavlovskii Exp $ */

#include <kernel/driver.h>
#include <kernel/fs.h>
#include <kernel/io.h>
#include <kernel/init.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/cache.h>

#define DEBUG
#include <kernel/debug.h>

#include <os/rtl.h>
#include <os/defs.h>
#include <os/syscall.h>

#include <errno.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>

//fsd_t *root;
vnode_t fs_root;

#include <kernel/proc.h>

typedef struct fs_mount_t fs_mount_t;
struct fs_mount_t
{
    fs_mount_t *prev, *next;
    vnode_t node;
    vnode_t root;
};

static spinlock_t sem_mounts;
fs_mount_t *fs_mount_first, *fs_mount_last;

static void FsCompletionApc(void *param)
{
    fs_asyncio_t *io;

    io = param;
    assert(io != NULL);
    /*wprintf(L"FsCompletionApc(%p): io->owner = %s/%d current = %s/%d\n", 
        io,
        io->owner->process->exe, io->owner->id, 
        current->process->exe, current->id);*/
    assert(io->owner == current());
    *io->original = io->op;
    io->original = NULL;
    HndUnlock(NULL, io->file_handle, 'file');
    EvtSignal(NULL, io->op.event);
    free(io);
}

void FsNotifyCompletion(fs_asyncio_t *io, size_t bytes, status_t result)
{
    io->op.result = result;
    io->op.bytes = bytes;
    io->file->pos += bytes;
    /*wprintf(L"FsNotifyCompletion(%p): io->owner = %p current = %p\n", 
        io, io->owner, current);*/

    if (io->owner == current())
        FsCompletionApc(io);
    else
    {
        wprintf(L"FsNotifyCompletion: queueing APC\n");
        ThrQueueKernelApc(io->owner, FsCompletionApc, io);
    }
}

static bool FsCheckAccess(file_t *file, uint32_t mask)
{
    return (file->flags & mask) == mask;
}

handle_t FsCreateFileHandle(process_t *proc, fsd_t *fsd, void *fsd_cookie, 
                            const wchar_t *name, uint32_t flags)
{
    handle_t fd;
    file_t *file;
    fs_info_t info;

    fd = HndAlloc(NULL, sizeof(file_t), 'file');
    file = HndLock(NULL, fd, 'file');
    if (file == NULL)
        return NULL;

    info.flags = FS_INFO_CACHE_BLOCK_SIZE;
    info.cache_block_size = 0;
    if (fsd->vtbl->get_fs_info != NULL)
        fsd->vtbl->get_fs_info(fsd, &info);

    file->fsd = fsd;
    file->fsd_cookie = fsd_cookie;

    if (info.cache_block_size != 0)
        file->cache = CcCreateFileCache(info.cache_block_size);
    else
        file->cache = NULL;

    /*if (name == NULL)
        file->name = NULL;
    else
        file->name = _wcsdup(name);*/

    file->flags = flags;
    file->pos = 0;
    HndUnlock(NULL, fd, 'file');
    return fd;
}

bool FsLookupMountPoint(vnode_t *out, const vnode_t *in)
{
    fs_mount_t *mount;

    SpinAcquire(&sem_mounts);

    for (mount = fs_mount_first; mount != NULL; mount = mount->next)
    {
        //wprintf(L"\tmount: (%p:%x) ", mount->node.fsd, mount->node.id);
        if (mount->node.fsd == in->fsd &&
            mount->node.id == in->id)
        {
            //wprintf(L"=> (%p:%x)\n", mount->root.fsd, mount->root.id);
            *out = mount->root;
            SpinRelease(&sem_mounts);
            return true;
        }
        //else
            //wprintf(L"\n");
    }

    SpinRelease(&sem_mounts);

    if (out != in)
        *out = *in;
    return false;
}

status_t FsPathToVnode(const wchar_t *path, vnode_t *vn, bool do_create, const wchar_t **out)
{
    wchar_t *name, *ch, *slash, *newname;
    status_t ret;
    vnode_t node, mount_point, old_node;

    //wprintf(L"FsPathToVnode: %s, ", path);
    ch = name = _wcsdup(path);

    if (do_create)
        *out = NULL;

    if (*ch == '/' || *ch == '\0')
    {
        while (*ch == '/')
            ch++;

        //node = current()->process->root;
        //if (node.fsd == NULL)
            node = fs_root;
        //wprintf(L"starting at root (%p:%x)\n", node.fsd, node.id);
    }
    else
        node = current()->process->cur_dir;

    assert(node.fsd != NULL);
    while (*ch != '\0')
    {
        slash = wcschr(ch, '/');
        if (slash == NULL)
        {
            if (do_create)
            {
                /*
                 * xxx - what do we do here if some FSD followed a link?
                 *  path no longer points to the full name.
                 */
                *out = path + (ch - name);
                break;
            }
        }
        else
            *slash = '\0';

        old_node = node;

        /*
         * If this is the root of a mounted FD, switch over to the mounted FS 
         *  unless the file name is "..".
         * If the file name is "..", give it to the FSD which handles the 
         *  directory under the mount point.
         */
        if (FsLookupMountPoint(&mount_point, &node) &&
            wcscmp(ch, L"..") != 0)
            node = mount_point;

        newname = NULL;
        //wprintf(L"element: %s", ch);
        assert(node.fsd != NULL);
        if (node.fsd->vtbl->parse_element == NULL)
            ret = ENOTIMPL;
        else
            ret = node.fsd->vtbl->parse_element(node.fsd, ch, &newname, &node);

        if (ret != 0)
        {
            //wprintf(L"error %d\n", ret);
            free(name);
            return ret;
        }

        if (newname != NULL)
        {
            free(name);
            ch = name = newname;
            node = old_node;

            if (*ch == '/' || *ch == '\0')
            {
                while (*ch == '/')
                    ch++;

                //node = current()->process->root;
                //if (node.fsd == NULL)
                    node = fs_root;
            }
        }
        else if (slash == NULL)
            ch = L"";
        else
            ch = slash + 1;

        //wprintf(L" => (%p:%x)\n", node.fsd, node.id);
    }

    free(name);
    *vn = node;
    //wprintf(L"FsPathToVnode(%s): (%p:%x)\n", path, node.fsd, node.id);
    return 0;
}

/*!
 *    \brief    Creates a file
 *
 *    \param    path    Full path specification for the file
 *    \param    flags    Bitmask of access flags for the file
 *    \sa    \p FILE_READ, \p FILE_WRITE
 *    \return    Handle to the new file
 */
handle_t FsCreateOpen(const wchar_t *path, uint32_t flags, bool do_create)
{
    void *cookie;
    vnode_t node;
    status_t ret;
    const wchar_t *nodename;

    ret = FsPathToVnode(path, &node, do_create, &nodename);
    if (ret == 0)
    {
        if (do_create && node.fsd->vtbl->create_file != NULL)
            ret = node.fsd->vtbl->create_file(node.fsd, node.id, nodename, &cookie);
        else if (!do_create && node.fsd->vtbl->lookup_file != NULL)
            ret = node.fsd->vtbl->lookup_file(node.fsd, node.id, &cookie);
        else
            ret = ENOTIMPL;
    }

    if (ret != 0)
    {
        errno = ret;
        return NULL;
    }

    return FsCreateFileHandle(NULL, node.fsd, cookie, L"", flags);
}

handle_t FsCreate(const wchar_t *path, uint32_t flags)
{
    return FsCreateOpen(path, flags, true);
}

handle_t FsOpen(const wchar_t *path, uint32_t flags)
{
    return FsCreateOpen(path, flags, false);
}

bool FsCreateDir(const wchar_t *path)
{
    status_t ret;
    void *dir_cookie;
    vnode_t node;
    const wchar_t *name;

    ret = FsPathToVnode(path, &node, true, &name);
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

    wprintf(L"FsCreateDir(%s): (%p:%x)\n", path, node.fsd, node.id);
    return true;
}

/*!
 *    \brief    Opens a directory search
 *
 *    \param    path    Full search specification
 *    \return    Handle to the search
 */
handle_t FsOpenDir(const wchar_t *path)
{
    //wchar_t fullname[256];
    status_t ret;
    //fsd_t *fsd;
    void *dir_cookie;
    handle_t fd;
    file_t *file;
    vnode_t node;

    //if (!FsFullPath(path, fullname))
        //return NULL;

    fd = HndAlloc(NULL, sizeof(file_t), 'file');
    file = HndLock(NULL, fd, 'file');

    /*fsd = root;
    if (fsd->vtbl->opendir == NULL)
        ret = ENOTIMPL;
    else
        ret = root->vtbl->opendir(root, fullname, &fsd, &dir_cookie);*/

    ret = FsPathToVnode(path, &node, false, NULL);
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
        HndUnlock(NULL, fd, 'file');
        HndClose(NULL, fd, 'file');
        return NULL;
    }

    wprintf(L"FsOpenDir(%s): (%p:%x)\n", path, node.fsd, node.id);
    file->fsd = node.fsd;
    file->fsd_cookie = dir_cookie;
    file->cache = NULL;
    file->flags = FILE_READ | FILE_IS_DIRECTORY;
    /*file->name = NULL;*/
    file->pos = 0;
    HndUnlock(NULL, fd, 'file');
    return fd;

    /*request_fs_t req;
    wchar_t fullname[256];

    if (!FsFullPath(path, fullname))
        return NULL;

    req.header.code = FS_OPENSEARCH;
    req.params.fs_opensearch.name = fullname;
    req.params.fs_opensearch.name_size = (wcslen(fullname) * sizeof(wchar_t)) + 1;
    req.params.fs_opensearch.file = NULL;
    req.params.fs_opensearch.flags = 0;
    if (IoRequestSync(root, (request_t*) &req))
        return req.params.fs_opensearch.file;
    else
    {
        errno = req.header.result;
        return NULL;
    }*/
}

bool FsReadDir(handle_t dir, dirent_t *di, size_t size)
{
    file_t *fd;
    fsd_t *fsd;
    status_t ret;

    if (!MemVerifyBuffer(di, size) ||
        size < sizeof(dirent_t))
    {
        errno = EBUFFER;
        return false;
    }

    fd = HndLock(NULL, dir, 'file');
    if (fd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    fsd = fd->fsd;

    if (!FsCheckAccess(fd, FILE_READ | FILE_IS_DIRECTORY))
    {
        HndUnlock(NULL, dir, 'file');
        errno = EACCESS;
        return false;
    }

    if (fsd->vtbl->readdir == NULL)
        ret = errno = ENOTIMPL;
    else
        ret = fsd->vtbl->readdir(fsd, fd->fsd_cookie, di);

    //if (ret == 0)
        //wprintf(L"FsReadDir: %s\n", di->name);

    HndUnlock(NULL, dir, 'file');
    return ret > 0 ? false : true;
}

/*!
 *    \brief    Closes a file
 *
 *    \param    file    Handle of the file to close
 *    \return    \p true if the handle was valid
 */
bool FsClose(handle_t file)
{
    file_t *fd;
    fsd_t *fsd;

    fd = HndLock(NULL, file, 'file');
    if (fd == NULL)
        return false;

    fsd = fd->fsd;
    if (fd->cache != NULL)
    {
        if (fsd->vtbl->flush_cache != NULL)
            fsd->vtbl->flush_cache(fsd, fd);

        CcDeleteFileCache(fd->cache);
        fd->cache = NULL;
    }

    /*free(fd->name);
    fd->name = NULL;*/

    if (fd->flags & FILE_IS_DIRECTORY)
    {
        if (fsd->vtbl->free_dir_cookie != NULL)
            fsd->vtbl->free_dir_cookie(fsd, fd->fsd_cookie);
    }
    else
    {
        if (fsd->vtbl->free_cookie != NULL)
            fsd->vtbl->free_cookie(fsd, fd->fsd_cookie);
    }

    HndUnlock(NULL, file, 'file');
    HndClose(NULL, file, 'file');
    return true;
}

/*!
 *    \brief    Reads from a file synchronously
 *
 *    This function will block until the read completes.
 *    \p errno will be updated with an error code if the read fails.
 *
 *    The file must have \p FILE_READ access.
 *
 *    \param    file    Handle of the file to read from
 *    \param    buf    Buffer to read into
 *    \param    bytes    Number of bytes to read
 *    \return    Number of bytes read
 */
bool FsReadSync(handle_t file, void *buf, size_t bytes, size_t *bytes_read)
{
    fileop_t op;

    op.event = file;
    op.bytes = 0;
    if (!FsRead(file, buf, bytes, &op))
    {
        errno = op.result;
        if (bytes_read != NULL)
            *bytes_read = op.bytes;
        return false;
    }

    if (op.result == SIOPENDING)
    {
        //wprintf(L"FsReadSync: op.result == SIOPENDING, waiting\n");
        ThrWaitHandle(current(), op.event, 0);
        KeYield();
        //wprintf(L"FsReadSync: finished wait\n");
    }

    errno = op.result;
    if (bytes_read != NULL)
        *bytes_read = op.bytes;

    return true;
}

/*!
 *    \brief    Writes to a file synchronously
 *
 *    This function will block until the write completes.
 *    \p errno will be updated with an error code if the write fails.
 *
 *    The file must have \p FILE_WRITE access.
 *
 *    \param    file    Handle of the file to write to
 *    \param    buf    Buffer to write out of
 *    \param    bytes    Number of bytes to write
 *    \return    Number of bytes written
 */
bool FsWriteSync(handle_t file, const void *buf, size_t bytes, size_t *bytes_written)
{
    fileop_t op;

    op.event = file;
    op.bytes = 0;
    if (!FsWrite(file, buf, bytes, &op))
    {
        errno = op.result;
        if (bytes_written != NULL)
            *bytes_written = op.bytes;
        return false;
    }

    if (op.result == SIOPENDING)
    {
        ThrWaitHandle(current(), op.event, 0);
        SysYield();
    }

    errno = op.result;
    if (bytes_written != NULL)
        *bytes_written = op.bytes;

    return true;
}

static bool FsReadWritePhysical(handle_t file, page_array_t *pages, size_t bytes, 
                                fileop_t *op, bool isRead)
{
    fsd_t *fsd;
    file_t *fd;
    fs_asyncio_t *io;
    bool ret;

    fd = HndLock(NULL, file, 'file');
    if (fd == NULL)
    {
        op->result = EHANDLE;
        return false;
    }

    fsd = fd->fsd;
    /*wprintf(L"FsReadWritePhysical(%p)\n", fsd);*/

    if (!FsCheckAccess(fd, isRead ? FILE_READ : FILE_WRITE))
    {
        HndUnlock(NULL, file, 'file');
        op->result = EACCESS;
        op->bytes = 0;
        return false;
    }

    if ((isRead && fsd->vtbl->read_file == NULL) ||
        (!isRead && fsd->vtbl->write_file == NULL))
    {
        HndUnlock(NULL, file, 'file');
        op->result = EACCESS;
        op->bytes = 0;
        return false;
    }

    io = malloc(sizeof(fs_asyncio_t));
    if (io == NULL)
    {
        HndUnlock(NULL, file, 'file');
        op->result = EACCESS;
        op->bytes = 0;
        return false;
    }

    op->result = 0;
    io->original = op;
    io->op = *op;
    io->owner = current();
    io->file = fd;
    io->file_handle = file;

    if (isRead)
    {
        /*wprintf(L"FsReadWritePhysical: reading %u bytes\n", bytes);*/
        ret = fsd->vtbl->read_file(fsd, fd, pages, bytes, io);
    }
    else
    {
        /*wprintf(L"FsReadWritePhysical: writing %u bytes\n", bytes);*/
        ret = fsd->vtbl->write_file(fsd, fd, pages, bytes, io);
    }

    /*wprintf(L"FsReadWritePhysical: ret = %d ", ret);*/
    if (!ret)
    {
        /*
         * Immediate failure: FsNotifyCompletion hasn't been called
         */
        /*op->result = ret;*/
        op->result = io->op.result;
        op->bytes = io->op.bytes;
        fd->pos += op->bytes;
        HndSignal(NULL, op->event, 0, true);
        HndUnlock(NULL, file, 'file');
        free(io);
        return false;
    }
    else
    {
        /*if (isRead)
            wprintf(L"FsReadWritePhysical: op->result = %d\n", op->result);*/

        /*
         * If the request is asynchronous, then that won't be reflected in ret.
         * It is up to the FSD to call FsNotifyCompletion for both async I/O 
         *  (the common case) and for sync I/O (less common, e.g. ramdisk).
         */

        return true;
    }
}

bool FsReadWrite(handle_t file, void *buf, size_t bytes, fileop_t *op, bool isRead)
{
    bool ret;
    page_array_t *pages;

    if (!MemVerifyBuffer(buf, bytes))
    {
        errno = op->result = EBUFFER;
        return false;
    }

    pages = MemCreatePageArray(buf, bytes);
    if (pages == NULL)
    {
        op->result = errno;
        return false;
    }

    ret = FsReadWritePhysical(file, pages, bytes, op, isRead);
    MemDeletePageArray(pages);
    return ret;
}

/*!
 *    \brief    Reads from a file asynchronously
 *
 *    The file must have \p FILE_READ access.
 *
 *    \param    file    Handle of the file to read from
 *    \param    buf    Buffer to read into
 *    \param    bytes    Number of bytes to read
 *    \param    op    \p fileop_t structure that receives the results of the read
 *    \return    \p true if the read could be started
 */
bool FsRead(handle_t file, void *buf, size_t bytes, struct fileop_t *op)
{
    return FsReadWrite(file, buf, bytes, op, true);
}

bool FsReadPhysical(handle_t file, page_array_t *pages, size_t bytes, struct fileop_t *op)
{
    return FsReadWritePhysical(file, pages, bytes, op, true);
}

/*!
 *    \brief    Writes to a file asynchronously
 *
 *    The file must have \p FILE_WRITE access.
 *
 *    \param    file    Handle of the file to read from
 *    \param    buf    Buffer to write out of
 *    \param    bytes    Number of bytes to write
 *    \param    op    \p fileop_t structure that receives the results of the write
 *    \return    \p true if the write could be started
 */
bool FsWrite(handle_t file, const void *buf, size_t bytes, struct fileop_t *op)
{
    return FsReadWrite(file, (void*) buf, bytes, op, false);
}

bool FsWritePhysical(handle_t file, page_array_t *pages, size_t bytes, struct fileop_t *op)
{
    return FsReadWritePhysical(file, pages, bytes, op, false);
}

/*!
 *    \brief    Seeks to a location in a file
 *
 *    Subsequent reads and writes will take place starting at the location
 *    specified.
 *    \param    file    Handle to the file to seek
 *    \param    ofs    New offset, relative to the beginning of the file
 *    \return    New offset, or 0 if the handle was invalid
 */
off_t FsSeek(handle_t file, off_t ofs, unsigned origin)
{
    file_t *fd;
    status_t ret;
    
    fd = HndLock(NULL, file, 'file');
    if (fd == NULL)
        return 0;

    switch (origin)
    {
    case FILE_SEEK_SET:
        break;

    case FILE_SEEK_CUR:
        ofs += fd->pos;
        break;

    case FILE_SEEK_END:
        {
            dirent_standard_t di;
            if (fd->fsd->vtbl->get_file_info == NULL)
                ret = ENOTIMPL;
            else
                ret = fd->fsd->vtbl->get_file_info(fd->fsd, fd->fsd_cookie, FILE_QUERY_STANDARD, &di);

            if (ret > 0)
            {
                ofs = fd->pos;
                errno = ret;
            }
            else
                ofs = di.length - ofs;
            break;
        }
    }

    fd->pos = ofs;
    HndUnlock(NULL, file, 'file');

    return ofs;
}

bool FsRequestSync(handle_t file, uint32_t code, void *params, size_t size, fileop_t *op)
{
    fsd_t *fsd;
    file_t *fd;
    fs_asyncio_t *io;

    fd = HndLock(NULL, file, 'file');
    if (fd == NULL)
    {
        op->result = EHANDLE;
        return false;
    }

    fsd = fd->fsd;

    if (fsd->vtbl->passthrough == NULL)
    {
        HndUnlock(NULL, file, 'file');
        op->result = EACCESS;
        op->bytes = 0;
        return false;
    }

    io = malloc(sizeof(fs_asyncio_t));
    if (io == NULL)
    {
        HndUnlock(NULL, file, 'file');
        op->result = EACCESS;
        op->bytes = 0;
        return false;
    }

    op->result = 0;
    op->event = file;
    io->original = op;
    io->op = *op;
    io->owner = current();
    io->file = fd;
    io->file_handle = file;

    /*wprintf(L"FsReadWritePhysical: ret = %d ", ret);*/
    if (!fsd->vtbl->passthrough(fsd, fd, code, params, size, io))
    {
        /*
         * Immediate failure: FsNotifyCompletion hasn't been called
         */
        /*op->result = ret;*/
        op->result = io->op.result;
        op->bytes = io->op.bytes;
        HndSignal(NULL, op->event, 0, true);
        HndUnlock(NULL, file, 'file');
        free(io);
        return false;
    }
    else
    {
        /*if (isRead)
            wprintf(L"FsReadWritePhysical: op->result = %d\n", op->result);*/

        /*
         * If the request is asynchronous, then that won't be reflected in ret.
         * It is up to the FSD to call FsNotifyCompletion for both async I/O 
         *  (the common case) and for sync I/O (less common, e.g. ramdisk).
         */

        return true;
    }

    /*file_t *fd;
    status_t ret;

    fd = HndLock(NULL, file, 'file');
    if (fd == NULL)
        return false;

    if (fd->fsd->vtbl->passthrough == NULL)
        ret = ENOTIMPL;
    else
        ret = fd->fsd->vtbl->passthrough(fd->fsd, fd, code, params, size, NULL);

    HndUnlock(NULL, file, 'file');
    return ret;*/
}

bool FsIoCtl(handle_t file, uint32_t code, void *buffer, size_t length, fileop_t *op)
{
    fsd_t *fsd;
    file_t *fd;
    fs_asyncio_t *io;
    
    fd = HndLock(NULL, file, 'file');
    if (fd == NULL)
    {
        op->result = EHANDLE;
        return false;
    }

    fsd = fd->fsd;

    if (fsd->vtbl->ioctl_file == NULL)
    {
        HndUnlock(NULL, file, 'file');
        op->result = EACCESS;
        op->bytes = 0;
        return false;
    }

    io = malloc(sizeof(fs_asyncio_t));
    if (io == NULL)
    {
        HndUnlock(NULL, file, 'file');
        op->result = EACCESS;
        op->bytes = 0;
        return false;
    }

    op->result = 0;
    op->event = file;
    io->original = op;
    io->op = *op;
    io->owner = current();
    io->file = fd;
    io->file_handle = file;

    /*wprintf(L"FsReadWritePhysical: ret = %d ", ret);*/
    if (!fsd->vtbl->ioctl_file(fsd, fd, code, buffer, length, io))
    {
        /*
         * Immediate failure: FsNotifyCompletion hasn't been called
         */
        /*op->result = ret;*/
        op->result = io->op.result;
        op->bytes = io->op.bytes;
        HndSignal(NULL, op->event, 0, true);
        HndUnlock(NULL, file, 'file');
        free(io);
        return false;
    }
    else
    {
        /*if (isRead)
            wprintf(L"FsReadWritePhysical: op->result = %d\n", op->result);*/

        /*
         * If the request is asynchronous, then that won't be reflected in ret.
         * It is up to the FSD to call FsNotifyCompletion for both async I/O 
         *  (the common case) and for sync I/O (less common, e.g. ramdisk).
         */

        return true;
    }

    /*file_t *fd;
    status_t ret;

    fd = HndLock(NULL, file, 'file');
    if (fd == NULL)
        return false;

    if (fd->fsd->vtbl->ioctl_file == NULL)
        ret = ENOTIMPL;
    else
        ret = fd->fsd->vtbl->ioctl_file(fd->fsd, fd, code, buffer, length, NULL);

    HndUnlock(NULL, file, 'file');
    return ret;*/
}

bool FsQueryFile(const wchar_t *name, uint32_t query_class, void *buffer, size_t buffer_size)
{
    void *cookie;
    status_t ret;
    vnode_t node;

    if (!MemVerifyBuffer(buffer, buffer_size))
    {
        errno = EBUFFER;
        return false;
    }

    //if (!FsFullPath(name, fullname))
        //return false;

    //wprintf(L"FsQueryFile(%s)\n", name);
    ret = FsPathToVnode(name, &node, false, NULL);
    if (ret == 0)
    {
        if (node.fsd->vtbl->lookup_file == NULL)
            ret = ENOTIMPL;
        else
        {
            ret = node.fsd->vtbl->lookup_file(node.fsd, node.id, &cookie);
        }
    }

    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    //wprintf(L"\tFsQueryFile(%s): (%p:%x)\n", name, node.fsd, node.id);
    if (node.fsd->vtbl->get_file_info == NULL)
        ret = ENOTIMPL;
    else
        ret = node.fsd->vtbl->get_file_info(node.fsd, cookie, query_class, buffer);

    if (node.fsd->vtbl->free_cookie != NULL)
        node.fsd->vtbl->free_cookie(node.fsd, cookie);

    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    return true;
}

bool FsQueryHandle(handle_t file, uint32_t query_class, void *buffer, size_t buffer_size)
{
    status_t ret;
    file_t *fd;

    if (!MemVerifyBuffer(buffer, buffer_size))
    {
        errno = EBUFFER;
        return false;
    }

    fd = HndLock(NULL, file, 'file');
    if (fd == NULL)
        return false;

    if (fd->fsd->vtbl->get_file_info == NULL)
        ret = ENOTIMPL;
    else
        ret = fd->fsd->vtbl->get_file_info(fd->fsd, fd->fsd_cookie, query_class, buffer);
    HndUnlock(NULL, file, 'file');

    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    return true;
}

static const struct
{
    const wchar_t *ext;
    const wchar_t *mimetype;
} mimetypes[] =
{
    { L"dll", L"application/x-pe-library" },
    { L"drv", L"application/x-pe-driver" },
    { L"exe", L"application/x-pe-program" },
    { L"gz", L"application/x-gzip" },
    { L"hqx", L"application/x-binhex40" },
    { L"lha", L"application/x-lharc" },
    { L"pcl", L"application/x-pcl" },
    { L"pdf", L"application/pdf" },
    { L"ps", L"application/postscript" },
    { L"sit", L"application/x-stuff-it" },
    { L"tar", L"application/x-tar" },
    { L"tgz", L"application/x-gzip" },
    { L"uue", L"application/x-uuencode" },
    { L"z", L"application/x-compress" },
    { L"zip", L"application/zip" },
    { L"zoo", L"application/x-zoo" },

    { L"aif", L"audio/x-aiff" },
    { L"aiff", L"audio/x-aiff" },
    { L"au", L"audio/basic" },
    { L"mid", L"audio/x-midi" },
    { L"midi", L"audio/x-midi" },
    { L"mod", L"audio/mod" },
    { L"ra", L"audio/x-real-audio" },
    { L"wav", L"audio/x-wav" },

    { L"bmp", L"image/x-bmp" },
    { L"fax", L"image/g3fax" },
    { L"gif", L"image/gif" },
    { L"iff", L"image/x-iff" },
    { L"jpg", L"image/jpeg" },
    { L"jpeg", L"image/jpeg" },
    { L"pbm", L"image/x-portable-bitmap" },
    { L"pcx", L"image/x-pcx" },
    { L"pgm", L"image/x-portable-graymap" },
    { L"png", L"image/png" },
    { L"ppm", L"image/x-portable-pixmap" },
    { L"rgb", L"image/x-rgb" },
    { L"tga", L"image/x-targa" },
    { L"tif", L"image/tiff" },
    { L"tiff", L"image/tiff" },
    { L"wmf", L"image/x-wmf" },
    { L"xbm", L"image/x-xbitmap" },

    { L"txt", L"text/plain" },
    { L"doc", L"text/plain" },
    { L"htm", L"text/html" },
    { L"html", L"text/html" },
    { L"rtf", L"text/rtf" },
    { L"asm", L"text/x-source-code" },
    { L"c", L"text/x-source-code" },
    { L"cc", L"text/x-source-code" },
    { L"c++", L"text/x-source-code" },
    { L"h", L"text/x-source-code" },
    { L"hh", L"text/x-source-code" },
    { L"cxx", L"text/x-source-code" },
    { L"cpp", L"text/x-source-code" },
    { L"S", L"text/x-source-code" },
    { L"java", L"text/x-source-code" },

    { L"avi", L"video/x-msvideo" },
    { L"mov", L"video/quicktime" },
    { L"mpg", L"video/mpeg" },
    { L"mpeg", L"video/mpeg" },

    { 0, 0 }
};

bool FsGuessMimeType(const wchar_t *ext, wchar_t *mimetype, size_t length)
{
    unsigned i;

    memset(mimetype, 0, sizeof(wchar_t) * length);
    if (ext != NULL)
    {
        if (ext[0] == '.')
            ext++;

        for (i = 0; mimetypes[i].ext != NULL; i++)
            if (_wcsicmp(ext, mimetypes[i].ext) == 0)
            {
                wcsncpy(mimetype, mimetypes[i].mimetype, length);
                return true;
            }
    }

    return false;
}

bool FsMountDevice(const wchar_t *path, fsd_t *newfsd)
{
    //request_fs_t req;
    status_t ret;
    vnode_t node;
    //const wchar_t *name;
    fs_mount_t *mount;

    if (wcscmp(path, L"/") == 0)
    {
        /*IoCloseDevice(root);*/
        if (fs_root.fsd != NULL &&
            fs_root.fsd->vtbl->dismount != NULL)
            fs_root.fsd->vtbl->dismount(fs_root.fsd);

        fs_root.fsd = newfsd;
        fs_root.id = VNODE_ROOT;
        wprintf(L"FsMountDevice(%s): mounting root = (%p:%x)\n", path, fs_root.fsd, fs_root.id);
        return true;
    }
    /*else if (fs_root.fsd->vtbl->mount != NULL)
    {
        ret = FsPathToVnode(path, &node, true, &name);
        if (ret == 0)
            ret = node.fsd->vtbl->mount(node.fsd, node.id, name, newfsd);
        if (ret != 0)
        {
            errno = ret;
            return false;
        }
        else
            return true;
    }
    else
    {
        errno = ENOTIMPL;
        return false;
    }*/
    else
    {
        ret = FsPathToVnode(path, &node, false, NULL);
        if (ret != 0)
        {
            errno = ret;
            return false;
        }

        mount = malloc(sizeof(fs_mount_t));
        if (mount == NULL)
            return false;

        memset(mount, 0, sizeof(fs_mount_t));
        mount->node = node;
        mount->root.fsd = newfsd;
        mount->root.id = VNODE_ROOT;
        SpinAcquire(&sem_mounts);
        LIST_ADD(fs_mount, mount);
        SpinRelease(&sem_mounts);
        wprintf(L"FsMountDevice(%s): mounting (%p:%x) = (%p:%x)\n", 
            path,
            mount->node.fsd, mount->node.id,
            mount->root.fsd, mount->root.id);
        return true;
    }

    /*{
        req.header.code = FS_MOUNT;
        req.params.fs_mount.name = path;
        req.params.fs_mount.name_size = (wcslen(path) * sizeof(wchar_t)) + 1;
        req.params.fs_mount.fsd = dev;
        if (IoRequestSync(root, (request_t*) &req))
            return true;
        else
        {
            errno = req.header.result;
            return false;
        }
    }*/
}

/*!
 *    \brief    Mounts a file system in a directory
 *
 *    The target directory must support the \p FS_MOUNT requests. Virtual
 *    folders (e.g. \p / and \p /System) support this.
 *
 *    \param    path    Full path specification of the new mount point
 *    \param    filesys    Name of the file system driver to use
 *    \param    dev    Device for the file system to use
 *    \return    \p true if the file system was mounted
 */
bool FsMount(const wchar_t *path, const wchar_t *filesys, const wchar_t *dest)
{
    driver_t *driver;
    fsd_t *fsd;

    driver = DevInstallNewDriver(filesys);
    if (driver == NULL)
    {
        wprintf(L"%s: unknown driver\n", filesys);
        return false;
    }

    if (driver->mount_fs == NULL)
    {
        DevUnloadDriver(driver);
        return false;
    }

    fsd = driver->mount_fs(driver, dest);
    if (fsd == NULL)
    {
        DevUnloadDriver(driver);
        return false;
    }

    assert(fsd->vtbl != NULL);
    if (!FsMountDevice(path, fsd))
    {
        if (fsd->vtbl->dismount != NULL)
            fsd->vtbl->dismount(fsd);
        return false;
    }
    else
        return true;
}

bool FsDismount(const wchar_t *path)
{
    /* xxx -- implement this */
    errno = ENOTIMPL;
    return false;
}

bool FsChangeDir(const wchar_t *path)
{
    status_t ret;
    vnode_t node;
    process_t *proc;

    proc = current()->process;
    if (!FsFullPath(path, proc->info->cwd))
        return false;

    wprintf(L"FsChangeDir: changing to %s = %s\n", path, proc->info->cwd);
    ret = FsPathToVnode(proc->info->cwd, &node, false, NULL);
    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    FsLookupMountPoint(&node, &node);
    proc->cur_dir = node;
    return true;
}

/*!
 *    \brief    Initializes the filing system
 *
 *    This function is called to mount any system and boot file systems. It 
 *    will mount:
 *    - The ramdisk on \p /System/Boot
 *    - The port file system on \p /System/Ports
 *    - The device file system on \p /System/Devices
 *    - (\p /System/Devices/ide0a on \p /hd using \p fat )
 *    - \p /System/Devices/fdc0 on \p /hd using \p fat
 *
 *    \return    \p true if all devices were mounted correctly
 */
bool FsInit(void)
{
    bool b;

    b = FsMount(L"/", L"vfs", NULL);
    assert(b || "Failed to mount VFS root");
    FsCreateDir(L"/System");
    FsCreateDir(SYS_BOOT);
    FsCreateDir(SYS_DEVICES);
    b = FsMount(SYS_BOOT, L"ramfs", NULL);
    assert(b || "Failed to mount ramdisk");
    //b = FsMount(SYS_PORTS, L"portfs", NULL);
    //assert(b || "Failed to mount ports");
    FsMount(SYS_DEVICES, L"devfs", NULL);
    assert(b || "Failed to mount devices");

    return true;
}
