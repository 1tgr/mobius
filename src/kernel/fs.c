/* $Id: fs.c,v 1.25 2002/05/19 13:04:36 pavlovskii Exp $ */

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

fsd_t *root;

#include <kernel/proc.h>

static void FsCompletionApc(void *param)
{
    fs_asyncio_t *io;

    io = param;
    assert(io != NULL);
    /*wprintf(L"FsCompletionApc(%p): io->owner = %s/%d current = %s/%d\n", 
        io,
        io->owner->process->exe, io->owner->id, 
        current->process->exe, current->id);*/
    assert(io->owner == current);
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

    if (io->owner == current)
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
    fsd_t *fsd;
    wchar_t fullname[256];
    status_t ret;
    void *cookie;

    if (!FsFullPath(path, fullname))
        return NULL;

    fsd = root;
    if (do_create && root->vtbl->create_file != NULL)
        ret = root->vtbl->create_file(root, fullname, &fsd, &cookie);
    else if (!do_create && root->vtbl->lookup_file != NULL)
        ret = root->vtbl->lookup_file(root, fullname, &fsd, &cookie);
    else
        ret = ENOTIMPL;

    if (ret != 0)
    {
        errno = ret;
        return NULL;
    }

    return FsCreateFileHandle(NULL, fsd, cookie, fullname, flags);
}

handle_t FsCreate(const wchar_t *path, uint32_t flags)
{
    return FsCreateOpen(path, flags, true);
}

handle_t FsOpen(const wchar_t *path, uint32_t flags)
{
    return FsCreateOpen(path, flags, false);
}

/*!
 *    \brief    Opens a directory search
 *
 *    \param    path    Full search specification
 *    \return    Handle to the search
 */
handle_t FsOpenDir(const wchar_t *path)
{
    wchar_t fullname[256];
    status_t ret;
    fsd_t *fsd;
    void *dir_cookie;
    handle_t fd;
    file_t *file;

    if (!FsFullPath(path, fullname))
        return NULL;

    fd = HndAlloc(NULL, sizeof(file_t), 'file');
    file = HndLock(NULL, fd, 'file');

    fsd = root;
    if (fsd->vtbl->opendir == NULL)
        ret = ENOTIMPL;
    else
        ret = root->vtbl->opendir(root, fullname, &fsd, &dir_cookie);

    if (ret != 0)
    {
        errno = ret;
        HndUnlock(NULL, fd, 'file');
        HndClose(NULL, fd, 'file');
        return NULL;
    }

    file->fsd = fsd;
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
        wprintf(L"FsReadSync: op.result == SIOPENDING, waiting\n");
        ThrWaitHandle(current, op.event, 0);
        KeYield();
        wprintf(L"FsReadSync: finished wait\n");
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
        ThrWaitHandle(current, op.event, 0);
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
    io->owner = current;
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
    io->owner = current;
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
    io->owner = current;
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
    wchar_t fullname[256];
    fsd_t *fsd;
    void *cookie;
    status_t ret;

    if (!MemVerifyBuffer(buffer, buffer_size))
    {
        errno = EBUFFER;
        return false;
    }

    if (!FsFullPath(name, fullname))
        return false;

    fsd = root;
    if (root->vtbl->lookup_file == NULL)
        ret = ENOTIMPL;
    else
        ret = root->vtbl->lookup_file(root, name, &fsd, &cookie);

    if (ret != 0)
    {
        errno = ret;
        return false;
    }

    if (fsd->vtbl->get_file_info == NULL)
        ret = ENOTIMPL;
    else
        ret = fsd->vtbl->get_file_info(fsd, cookie, query_class, buffer);

    if (fsd->vtbl->free_cookie != NULL)
        fsd->vtbl->free_cookie(fsd, cookie);

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
    
    if (wcscmp(path, L"/") == 0)
    {
        /*IoCloseDevice(root);*/
        if (root != NULL &&
            root->vtbl->dismount != NULL)
            root->vtbl->dismount(root);

        root = newfsd;
        return true;
    }
    else if (root->vtbl->mount != NULL)
    {
        ret = root->vtbl->mount(root, path, newfsd);
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

    FsCreateVirtualDir(L"/");
    FsCreateVirtualDir(L"/System");
    b = FsMount(SYS_BOOT, L"ram", NULL);
    assert(b || "Failed to mount ramdisk");
    b = FsMount(SYS_PORTS, L"portfs", NULL);
    assert(b || "Failed to mount ports");
    FsMount(SYS_DEVICES, L"devfs", NULL);
    assert(b || "Failed to mount devices");

    return true;
}
