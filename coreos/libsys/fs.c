/*
 * $History: fs.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 7/03/04    Time: 13:54
 * Updated in $/coreos/libsys
 * Added FsAllocSyncIoEvent and FsFreeSyncIoEvent
 * Added history block
 */

#include <os/syscall.h>
#include <os/rtl.h>
#include <os/defs.h>
#include <errno.h>

/*
 * Allocate an event handle for a sync I/O operation
 */
static handle_t FsAllocSyncIoEvent()
{
    thread_info_t *info;
    unsigned index;

    info = ThrGetThreadInfo();
    index = info->next_sync_io_event++;
    return info->sync_io_events[index];
}


/*
 * Deallocate an event handle used for a sync I/O operation
 */
static void FsFreeSyncIoEvent(handle_t event)
{
    thread_info_t *info;
    info = ThrGetThreadInfo();
    info->next_sync_io_event--;
}


bool FsRead(handle_t file, void *buf, uint64_t offset, size_t bytes, size_t *bytes_read)
{
    fileop_t op;
    status_t ret;

    op.event = FsAllocSyncIoEvent();
    ret = FsReadAsync(file, buf, offset, bytes, &op);
    if (ret > 0)
    {
        FsFreeSyncIoEvent(op.event);
        errno = ret;
        return false;
    }
    else if (ret == SIOPENDING)
    {
        if (!ThrWaitHandle(op.event))
            DbgWrite(L"FsRead: ThrWaitHandle failed\n", 21);
        ret = op.result;
    }

    FsFreeSyncIoEvent(op.event);
    if (bytes_read != 0)
        *bytes_read = op.bytes;

    if (ret != 0)
    {
        if (ret == SIOPENDING)
            DbgWrite(L"FsRead: still pending\n", 22);
        errno = ret;
        return false;
    }
    else
        return true;
}


bool FsWrite(handle_t file, const void *buf, uint64_t offset, size_t bytes, size_t *bytes_written)
{
    fileop_t op;
    status_t ret;

    op.event = FsAllocSyncIoEvent();
    ret = FsWriteAsync(file, buf, offset, bytes, &op);
    if (ret > 0)
    {
        FsFreeSyncIoEvent(op.event);
        errno = ret;
        return false;
    }
    else if (ret == SIOPENDING)
    {
        ThrWaitHandle(op.event);
        ret = op.result;
    }

    FsFreeSyncIoEvent(op.event);
    if (bytes_written != 0)
        *bytes_written = op.bytes;

    if (ret != 0)
    {
        errno = ret;
        return false;
    }
    else
        return true;
}


bool FsRequestSync(handle_t file, uint32_t code, void *buf, size_t bytes, size_t *bytes_out)
{
    fileop_t op;

    op.event = FsAllocSyncIoEvent();
    if (!FsRequest(file, code, buf, bytes, &op))
    {
        FsFreeSyncIoEvent(op.event);
        errno = op.result;
        return false;
    }

    while (op.result == SIOPENDING)
        ThrWaitHandle(op.event);

    FsFreeSyncIoEvent(op.event);

    if (op.result != 0)
    {
        errno = op.result;
        return false;
    }

    if (bytes_out != 0)
        *bytes_out = op.bytes;

    return true;
}


bool FsGetFileLength(handle_t file, uint64_t *length)
{
    dirent_standard_t di;

    if (FsQueryHandle(file, FILE_QUERY_STANDARD, &di, sizeof(di)))
    {
        *length = di.length;
        return true;
    }
    else
        return false;
}
