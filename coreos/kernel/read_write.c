/*
 * $History: read_write.c $
 * 
 * *****************  Version 1  *****************
 * User: Tim          Date: 14/03/04   Time: 1:08
 * Created in $/coreos/kernel
 */

#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <kernel/thread.h>
#include <kernel/cache.h>

#include <kernel/debug.h>

#include <string.h>
#include <errno.h>
#include <wchar.h>

/*
 * Allocate an event handle for a sync I/O operation
 */
static handle_t FsAllocSyncIoEvent()
{
    thread_info_t *info;
    unsigned index;

    info = current()->info;
    index = KeAtomicInc(&info->next_sync_io_event);
    assert(index < _countof(info->sync_io_events));
    return info->sync_io_events[index];
}


/*
 * Deallocate an event handle used for a sync I/O operation
 */
static void FsFreeSyncIoEvent(handle_t event)
{
    thread_info_t *info;
    unsigned index;

    info = current()->info;
    index = KeAtomicDec(&info->next_sync_io_event);
    assert(info->sync_io_events[index - 1] == event);
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
bool FsRead(file_handle_t *file, void *buf, uint64_t offset, size_t bytes, size_t *bytes_read)
{
    fileop_t op;
    status_t ret;

    op.event = FsAllocSyncIoEvent();
    assert(op.event != 0);
    op.bytes = 0;
    ret = FsReadAsync(file, buf, offset, bytes, &op);
    if (ret > 0)
    {
        FsFreeSyncIoEvent(op.event);
        errno = ret;
        if (bytes_read != NULL)
            *bytes_read = op.bytes;
        return false;
    }
    else if (ret == SIOPENDING)
    {
        event_t *event;
        event = (event_t*) HndRef(NULL, op.event, 'evnt');
        ThrWaitHandle(current(), &event->hdr);
        KeYield();
        ret = op.result;
    }

    errno = ret;
    if (bytes_read != NULL)
        *bytes_read = op.bytes;

    FsFreeSyncIoEvent(op.event);
    return ret == 0;
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
bool FsWrite(file_handle_t *file, const void *buf, uint64_t offset, size_t bytes, size_t *bytes_written)
{
    fileop_t op;
    status_t ret;

    op.event = FsAllocSyncIoEvent();
    assert(op.event != 0);
    op.bytes = 0;
    ret = FsWriteAsync(file, buf, offset, bytes, &op);
    if (ret > 0)
    {
        FsFreeSyncIoEvent(op.event);
        errno = ret;
        if (bytes_written != NULL)
            *bytes_written = op.bytes;
        return false;
    }
    else if (ret == SIOPENDING)
    {
        event_t *event;
        event = (event_t*) HndRef(NULL, op.event, 'evnt');
        ThrWaitHandle(current(), &event->hdr);
        KeYield();
        ret = op.result;
    }

    errno = ret;
    if (bytes_written != NULL)
        *bytes_written = op.bytes;

    FsFreeSyncIoEvent(op.event);
    return ret == 0;
}

CASSERT(ENCLOSING_STRUCT((void*) 0x1234, file_handle_t, hdr) == (file_handle_t*) 0x1234);


/*
 * Create an fs_asyncio_t object
 */
static fs_asyncio_t *FsCreateAsyncIo(file_t *fd, 
                                     page_array_t *pages,
                                     uint64_t pos,
                                     size_t length,
                                     fileop_t *op,
                                     bool is_reading)
{
    fs_asyncio_t *io;

    io = malloc(sizeof(*io));
    if (io == NULL)
        return NULL;

    memset(io, 0, sizeof(*io));

    io->original = op;
    io->op = *op;
    io->op.bytes = 0;
    io->owner = current();
    io->request.file = FsReferenceFile(fd);
    io->request.pos = pos;

    if (pages == NULL)
        io->request.pages = NULL;
    else
        io->request.pages = MemCopyPageArray(pages);

    io->request.length = length;
    io->request.is_reading = is_reading;
    io->request.io = io;

    return io;
}


/*
 * Delete an fs_asyncio_t object
 */
static void FsDeleteAsyncIo(fs_asyncio_t *io)
{
    if (io->request.pages != NULL)
        MemDeletePageArray(io->request.pages);
    FsReleaseFile(io->request.file);
    free(io);
}


/*
 * Worker function to notify the originator after an I/O operation has 
 *  completed, then clean up
 */
static void FsDoCompletion(fs_asyncio_t *io)
{
    event_t *evt;

    // Only execute in the context of the originator. FsNotifyCompletion 
    //  queues an APC to ensure this, if necessary.
    assert(io->owner->process == current()->process);

    // Propagate the byte count and result code back to the originator. This 
    //  is why we have the assert above...
    *io->original = io->op;

    // Tell the originator we've finished
    evt = (event_t*) HndRef(io->owner->process, io->op.event, 'evnt');
    assert(evt != NULL);
    EvtSignal(evt, true);

    // Clean up
    FsDeleteAsyncIo(io);
}


/*
 * APC routine to call FsDoCompletion
 */
static void FsCompletionApc(void *param)
{
    fs_asyncio_t *io;

    io = param;
    assert(io != NULL);
    FsDoCompletion(param);
}


/*
 * Perform the next bit of I/O for a request on a cached file
 */
static void FsStartIo(fs_asyncio_t *io)
{
    unsigned bytes, block_mod;
    uint8_t *buf, *page;
    page_array_t *array;
    status_t ret;
    fs_request_t req;
    size_t bytes_read, bytes_to_copy;

    // Hooray! A label! There's a goto down there somewhere...
start:

    // Have we finished with an error, or transferred enough bytes?
    if (io->op.result > 0 ||
        io->op.bytes >= io->request.length)
    {
        // ...yes, so branch to the cleanup code
        wprintf(L"FsStartIo: finished, result = %d, bytes = %u\n",
            io->op.result,
            io->op.bytes);
        goto finished;
    }

    // Try to read as many bytes as we can...
    bytes = io->request.length - io->op.bytes;
    // ...but don't cross a block boundary
    block_mod = (io->request.pos + io->op.bytes) & (io->request.file->cache->block_size - 1);
    if (block_mod + bytes > io->request.file->cache->block_size)
        bytes = io->request.file->cache->block_size - block_mod;

    // Is this block already in the cache?
    if (CcIsBlockValid(io->request.file->cache, io->request.pos + io->op.bytes))
    {
        /*
         * ...yes, so memcpy into/out of it
         */

        TRACE3("FsStartIo: cached: pos = %x, bytes = %u, block_mod = %u\n",
            (unsigned) io->request.pos,
            bytes,
            block_mod);

        // Map the originator's buffer
        buf = MemMapPageArray(io->request.pages);

        // Create a page array to represent the relevant part of the cache
        array = CcRequestBlock(io->request.file->cache, 
            io->request.pos + io->op.bytes, 
            &bytes_read);
        // Map it
        page = MemMapPageArray(array);

        bytes_to_copy = min(bytes_read, bytes);
        TRACE2("\tbytes_read = %u, bytes_to_copy = %u\n", bytes_read, bytes_to_copy);

        // Are we reading?
        if (io->request.is_reading)
            // ...yes, so copy from the cache to the originator's buffer
            memcpy(buf + io->op.bytes,
                page + block_mod,
                bytes_to_copy);
        else
            // ...no, so copy from the originator's buffer to the cache
            memcpy(page + block_mod,
                buf + io->op.bytes,
                bytes_to_copy);

        // Unmap the originator's buffer
        MemUnmapPageArray(io->request.pages);

        // Unmap the cache
        MemUnmapPageArray(array);
        // Tell the cache that this part is now valid, and mark it dirty if
        //  we just wrote to it
        CcReleaseBlock(io->request.file->cache, 
            io->request.pos + io->op.bytes, 
            bytes_read,
            true, 
            !io->request.is_reading);
        MemDeletePageArray(array);

        // We've transferred some bytes
        io->op.bytes += bytes_to_copy;

        if (bytes_to_copy < bytes ||
            io->op.bytes >= io->request.length)
        {
            // We've finished, so we'll clean up.
            io->op.result = 0;
            goto finished;
        }
        else
        {
            /*
             * More data need to be read.
             */
            goto start;
        }
    }
    else
    {
        uint64_t pos;

        /*
         * The cluster is not cached, so we need to lock the relevant
         *    cache block and read the entire cluster into there.
         * When the read has finished we'll try the cache again.
         */

        pos = (io->request.pos + io->op.bytes) & -io->request.file->cache->block_size;
        TRACE2("FsStartIo: not cached: pos = %x, block_mod = %u\n",
            (unsigned) pos,
            block_mod);

        // Create a page array to represent the relevant part of the cache
        array = CcRequestBlock(io->request.file->cache, pos, NULL);

        req.file = io->request.file;
        req.pages = array;
        req.pos = pos;
        req.length = io->request.file->cache->block_size;
        req.io = io;
        req.is_reading = io->request.is_reading;

        // Ask the FSD to do its stuff
        ret = io->request.file->vnode.fsd->vtbl->read_write_file(io->request.file->vnode.fsd, 
            &req,
            &bytes);

        // We don't care about the cache page array any more, though if the 
        //  FSD needs it, it will have called MemCopyPageArray
        MemDeletePageArray(array);

        // Did the FSD signal an error?
        if (ret > 0)
        {
            // ...yes, so finish now
            wprintf(L"FsStartIo: read of %u bytes failed: %d\n", 
                io->request.file->cache->block_size, ret);
            io->op.result = ret;
            io->op.bytes += bytes;
            goto finished;
        }
    }

    return;

finished:
    /*
     * We've finished all the I/O, so tell the originator about it
     */
    if (io->owner->process == current()->process)
        FsDoCompletion(io);
    else
        ThrQueueKernelApc(io->owner, FsCompletionApc, io);
}


/*
 * Called by an FSD once it's finished some I/O given to it
 */
void FsNotifyCompletion(fs_asyncio_t *io, size_t bytes, status_t result)
{
    // Is this file cached?
    if (io->request.file->cache == NULL)
    {
        // ...no, so completion means that the operation has completely finished

        io->op.result = result;
        io->op.bytes += bytes;

        if (io->owner->process == current()->process)
            FsDoCompletion(io);
        else
            ThrQueueKernelApc(io->owner, FsCompletionApc, io);
    }
    else
    {
        /*
         * ...yes, and we've just finished transferring data from the file 
         *  system into the cache
         */

        TRACE2("FsNotifyCompletion: result = %d, bytes = %u\n", result, bytes);

        // Was the FSD successful?
        if (result == 0)
        {
            uint64_t pos;

            // ...yes, so the block is now valid
            pos = (io->request.pos + io->op.bytes) & -io->request.file->cache->block_size;
            CcReleaseBlock(io->request.file->cache, 
                pos, 
                bytes,
                true, 
                !io->request.is_reading);
        }

        // Do some more I/O. If the last operation was successful, FsStartIo 
        //  will continue to the next block. If not, FsStartIo will complete
        //  the operation and clean up as necessary.
        io->op.result = result;
        FsStartIo(io);
    }
}


/*
 * Helper function to check whether the flags in mask are set in a file handle
 */
static bool FsCheckAccess(file_handle_t *fh, uint32_t mask)
{
    return (fh->flags & mask) == mask;
}


/*
 * Core async read/write routine 
 */
static status_t FsReadWritePhysicalAsync(file_handle_t *fh, page_array_t *pages, 
                                         uint64_t offset, size_t bytes, 
                                         fileop_t *op, bool isRead)
{
    event_t *event;
    fsd_t *fsd;
    file_t *fd;
    fs_asyncio_t *io;
    status_t ret;

    // Are we allowed to do what we're about to do?
    if (!FsCheckAccess(fh, isRead ? FILE_READ : FILE_WRITE))
    {
        // ...no, so fail early on
        op->bytes = 0;
        return op->result = EACCESS;
    }

    // Grab the file_t object from the file handle
    fd = fh->file;

    // Grab the FSD
    fsd = fd->vnode.fsd;

    // Can this FSD read or write files?
    if (fsd->vtbl->read_write_file == NULL)
    {
        // ...no, so fail
        op->bytes = 0;
        return op->result = EACCESS;
    }

    // Attempt to validate the event object at the beginning
    event = (event_t*) HndRef(NULL, op->event, 'evnt');
    if (event == NULL)
        return op->result = errno;

    // Make sure it's not already signalled
    EvtSignal(event, false);

    // Clear any outstanding error code
    op->result = 0;

    // Bundle the parameters into an fs_asyncio_t object
    io = FsCreateAsyncIo(fd, pages, offset, bytes, op, isRead);
    if (io == NULL)
    {
        op->bytes = 0;
        return op->result = errno;
    }

    // Is this file cached?
    if (fd->cache == NULL)
    {
        // ...no, so do all the I/O now
        bytes = 0;
        ret = fsd->vtbl->read_write_file(fsd, &io->request, &bytes);
    }
    else
    {
        // ...yes, so ask FsStartIo to begin
        FsStartIo(io);

        // This is slightly pessimistic. If the FSD processes the operation 
        //  synchronously, FsStartIo will ultimately call FsNotifyCompletion 
        //  and the whole operation will be over by this point. Unfortunately,
        //  if that happens, io will have been freed, so we can't check. By
        //  returning SIOPENDING, we force the originator to wait on its event
        //  anyway; if the I/O has already completed, the wait will finish
        //  immediately.
        // The upshot of this is that, for cached files, FsReadAsync and 
        //  FsWriteAsync always return either SIOPENDING, or an error code.
        ret = SIOPENDING;
    }

    if (ret > 0)
    {
        /*
         * Immediate failure: FsNotifyCompletion hasn't been called
         */

        FsNotifyCompletion(io, bytes, ret);
    }

    return ret;
}


static status_t FsReadWriteAsync(file_handle_t *file, void *buf, 
                                 uint64_t offset, size_t bytes, 
                                 fileop_t *op, bool isRead)
{
    status_t ret;
    page_array_t *pages;

    if (!MemVerifyBuffer(buf, bytes))
        return errno = op->result = EBUFFER;

    pages = MemCreatePageArray(buf, bytes);
    if (pages == NULL)
        return op->result = errno;

    ret = FsReadWritePhysicalAsync(file, pages, offset, bytes, op, isRead);
    MemDeletePageArray(pages);

    if (ret > 0)
        errno = ret;

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
status_t FsReadAsync(file_handle_t *file, void *buf, uint64_t offset, 
                     size_t bytes, fileop_t *op)
{
    return FsReadWriteAsync(file, buf, offset, bytes, op, true);
}


status_t FsReadPhysicalAsync(file_handle_t *file, page_array_t *pages, 
                             uint64_t offset, size_t bytes, fileop_t *op)
{
    return FsReadWritePhysicalAsync(file, pages, offset, bytes, op, true);
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
status_t FsWriteAsync(file_handle_t *file, const void *buf, uint64_t offset, 
                      size_t bytes, fileop_t *op)
{
    return FsReadWriteAsync(file, (void*) buf, offset, bytes, op, false);
}


status_t FsWritePhysicalAsync(file_handle_t *file, page_array_t *pages, 
                              uint64_t offset, size_t bytes, fileop_t *op)
{
    return FsReadWritePhysicalAsync(file, pages, offset, bytes, op, false);
}


bool FsRequest(file_handle_t *fh, uint32_t code, void *params, size_t size, fileop_t *op)
{
    fsd_t *fsd;
    file_t *fd;
    fs_asyncio_t *io;

    fd = fh->file;
    fsd = fd->vnode.fsd;

    if (fsd->vtbl->passthrough == NULL)
    {
        op->result = EACCESS;
        op->bytes = 0;
        return false;
    }

    op->result = 0;
    io = FsCreateAsyncIo(fd, NULL, 0, 0, op, false);
    if (io == NULL)
    {
        op->bytes = 0;
        op->result = errno;
        return op->result;
    }

    if (!fsd->vtbl->passthrough(fsd, fd, code, params, size, io))
    {
        /*
         * Immediate failure: FsNotifyCompletion hasn't been called
         */
        FsNotifyCompletion(io, 0, io->op.result);
        return false;
    }
    else
    {
        /*
         * If the request is asynchronous, then that won't be reflected in ret.
         * It is up to the FSD to call FsNotifyCompletion for both async I/O 
         *  (the common case) and for sync I/O (less common, e.g. ramdisk).
         */

        return true;
    }
}


bool FsIoCtl(file_handle_t *fh, uint32_t code, void *buffer, size_t length, fileop_t *op)
{
    fsd_t *fsd;
    file_t *fd;
    fs_asyncio_t *io;

    fd = fh->file;
    fsd = fd->vnode.fsd;

    if (fsd->vtbl->ioctl_file == NULL)
    {
        op->result = EACCESS;
        op->bytes = 0;
        return false;
    }

    op->result = 0;
    io = FsCreateAsyncIo(fd, NULL, 0, 0, op, false);
    if (io == NULL)
    {
        op->bytes = 0;
        op->result = errno;
        return op->result;
    }

    if (!fsd->vtbl->ioctl_file(fsd, fd, code, buffer, length, io))
    {
        /*
         * Immediate failure: FsNotifyCompletion hasn't been called
         */
        FsNotifyCompletion(io, io->op.bytes, io->op.result);
        return false;
    }
    else
    {
        /*
         * If the request is asynchronous, then that won't be reflected in ret.
         * It is up to the FSD to call FsNotifyCompletion for both async I/O 
         *  (the common case) and for sync I/O (less common, e.g. ramdisk).
         */

        return true;
    }
}
