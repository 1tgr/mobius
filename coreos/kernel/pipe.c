/*
 * $History: pipe.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 14:01
 * Updated in $/coreos/kernel
 * Removed unneeded fields from pipe_asyncio_t structure
 * Added history block
 */

#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/proc.h>

#include <os/ioctl.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct pipe_asyncio_t pipe_asyncio_t;
struct pipe_asyncio_t
{
    pipe_asyncio_t *prev, *next;
    fs_asyncio_t *io;
};

struct pipe_t
{
    pipe_t *other;
    bool is_closed;
    uint8_t *buffer;
    unsigned write_ptr, read_ptr, allocated;
    pipe_asyncio_t *aio_first, *aio_last;
    spinlock_t sem;
};

static void PipeWakeBlockedReaders(pipe_t *pipe)
{
    uint8_t *dest;
    pipe_asyncio_t *pipe_io, *next;
    fs_asyncio_t *io;
    size_t length;

    length = pipe->write_ptr - pipe->read_ptr;
    //wprintf(L"PipeWakeBlockedReaders(%p): %u bytes\n", pipe, length);

    SpinAcquire(&pipe->sem);
    for (pipe_io = pipe->aio_first; pipe_io != NULL; pipe_io = next)
    {
        next = pipe_io->next;
        io = pipe_io->io;

        if (length > 0)
        {
            LIST_REMOVE(pipe->aio, pipe_io);
            free(pipe_io);

            length = pipe->write_ptr - pipe->read_ptr;
            if (length > io->request.length)
                length = io->request.length;
            //wprintf(L"Pipe has %u bytes, request %p needs %u bytes\n",
                //length, io, io->request.length);
            dest = MemMapPageArray(io->request.pages);
            memcpy(dest, pipe->buffer + pipe->read_ptr, length);
            MemUnmapPageArray(io->request.pages);
            pipe->read_ptr += length;

            SpinRelease(&pipe->sem);
            FsNotifyCompletion(io, length, 0);
            SpinAcquire(&pipe->sem);
        }
        else if (pipe->other->is_closed)
        {
            LIST_REMOVE(pipe->aio, pipe_io);
            free(pipe_io);

            SpinRelease(&pipe->sem);
            FsNotifyCompletion(io, 0, EPIPECLOSED);
            SpinAcquire(&pipe->sem);
        }
    }

    SpinRelease(&pipe->sem);
}


void PipeFreeCookie(fsd_t *fsd, void *cookie)
{
    pipe_t *pipe;

    pipe = cookie;
    SpinAcquire(&pipe->sem);
    assert(!pipe->is_closed);
    pipe->is_closed = true;
    free(pipe->buffer);
    pipe->buffer = NULL;
    pipe->allocated = 0;
    pipe->read_ptr = pipe->write_ptr = 0;

    if (pipe->other->is_closed)
    {
        wprintf(L"PipeFreeCookie(%p): both ends closed\n", pipe);
        free(pipe->other);
        SpinRelease(&pipe->sem);
        free(pipe);
    }
    else
    {
        wprintf(L"PipeFreeCookie(%p): other end still open\n", pipe);
        SpinRelease(&pipe->sem);
        PipeWakeBlockedReaders(pipe->other);
    }
}


status_t PipeReadWriteFile(fsd_t *fsd, const fs_request_t *req, size_t *bytes)
{
    if (req->is_reading)
    {
        pipe_t *pipe;
        pipe_asyncio_t *pipe_io;

        /*
         * Don't use any fields in file other than fsd_cookie, because it might
         *  come from PortReadFile, which doesn't bother filling in the other 
         *  fields.
         */
        pipe = req->file->fsd_cookie;
        assert(!pipe->is_closed);

        if (pipe->aio_first != NULL)
        {
            pipe_asyncio_t *aio_first;
            fs_asyncio_t *io;
            thread_t *owner;
            process_t *process;

            aio_first = pipe->aio_first;
            io = aio_first->io;
            owner = io->owner;
            process = owner->process;

            /* xxx -- only allow one reader for now */
            wprintf(L"PipeReadWriteFile(%s/%s, %p): already got a reader queued (%s/%s)\n", 
                req->io->owner->process->exe,
                req->io->owner->name,
                pipe,
                process->exe,
                owner->name);
            return EACCESS;
        }

        pipe_io = malloc(sizeof(pipe_asyncio_t));
        if (pipe_io == NULL)
            return errno;

        memset(pipe_io, 0, sizeof(*pipe_io));
        pipe_io->io = req->io;

        SpinAcquire(&pipe->sem);
        LIST_ADD(pipe->aio, pipe_io);
        SpinRelease(&pipe->sem);

        PipeWakeBlockedReaders(pipe);
        return SIOPENDING;
    }
    else
    {
        pipe_t *pipe, *other;
        void *buf;

        /*
         * Same proviso applies here as in PipeReadFile about the other fields in
         *  file.
         */
        pipe = req->file->fsd_cookie;
        assert(!pipe->is_closed);
        other = pipe->other;

        if (other->is_closed)
            return EPIPECLOSED;

        SpinAcquire(&pipe->other->sem);
        if (other->write_ptr + req->length > other->allocated)
        {
            /*wprintf(L"PipeWriteFile(%p): read = %u, write = %u, length = %u, allocated = %u\n",
                pipe,
                other->read_ptr,
                other->write_ptr,
                length,
                other->allocated);*/

            if (other->read_ptr == other->write_ptr)
                other->read_ptr = other->write_ptr = 0;

            if (other->write_ptr + req->length > other->allocated)
            {
                other->allocated = (other->write_ptr + req->length + 15) & -16;
                other->buffer = realloc(other->buffer, other->allocated);
                TRACE2(L"PipeWriteFile(%p): reallocated to %u bytes\n", 
                    pipe, other->allocated);
            }
        }

        assert(other->write_ptr + req->length <= other->allocated);

        buf = MemMapPageArray(req->pages);
        memcpy(other->buffer + other->write_ptr, buf, req->length);
        MemUnmapPageArray(req->pages);

        other->write_ptr += req->length;
        SpinRelease(&other->sem);
        FsNotifyCompletion(req->io, req->length, 0);

        PipeWakeBlockedReaders(other);
        return 0;
    }
}


bool PipeIoCtlFile(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
                   size_t length, fs_asyncio_t *io)
{
    pipe_t *pipe;

    pipe = file->fsd_cookie;
    switch (code)
    {
    case IOCTL_BYTES_AVAILABLE:
        if (length < sizeof(size_t))
        {
            io->op.result = EBUFFER;
            return false;
        }

        //wprintf(L"PipeIoCtlFile(IOCTL_BYTES_AVAILABLE): %u\n",
            //pipe->write_ptr - pipe->read_ptr);
        *(size_t*) buf = pipe->write_ptr - pipe->read_ptr;
        break;

    default:
        io->op.result = ENOTIMPL;
        return false;
    }

    FsNotifyCompletion(io, length, 0);
    return true;
}


static vtbl_fsd_t pipe_vtbl =
{
    NULL,           /* dismount */
    NULL,           /* get_fs_info */
    NULL,           /* parse_element */
    NULL,           /* create_file */
    NULL,           /* lookup_file */
    NULL,           /* get_file_info */
    NULL,           /* set_file_info */
    PipeFreeCookie,
    PipeReadWriteFile,
    PipeIoCtlFile,
    NULL,           /* passthrough */
    NULL,           /* mkdir */
    NULL,           /* opendir */
    NULL,           /* readdir */
    NULL,           /* freedircookie */
    NULL,           /* finishio */
    NULL,           /* flush_cache */
};


fsd_t pipe_fsd = 
{
    &pipe_vtbl,
};

bool FsCreatePipeInternal(pipe_t **pipes)
{
    pipes[0] = malloc(sizeof(pipe_t));
    if (pipes[0] == NULL)
        return false;

    memset(pipes[0], 0, sizeof(pipe_t));

    pipes[1] = malloc(sizeof(pipe_t));
    if (pipes[1] == NULL)
    {
        free(pipes[0]);
        return false;
    }

    memset(pipes[1], 0, sizeof(pipe_t));
    pipes[0]->other = pipes[1];
    pipes[1]->other = pipes[0];

    return true;
}


bool FsCreatePipe(file_handle_t **ends)
{
    file_t *files[2];
    pipe_t *pipes[2];

    if (!FsCreatePipeInternal(pipes))
        return false;

    files[0] = FsCreateFileObject(&pipe_fsd, VNODE_NONE, pipes[0]);
    files[1] = FsCreateFileObject(&pipe_fsd, VNODE_NONE, pipes[1]);

    ends[0] = FsCreateFileHandle(files[0], FILE_READ | FILE_WRITE);
    if (ends[0] == NULL)
    {
        FsReleaseFile(files[0]);
        FsReleaseFile(files[1]);
        free(pipes[0]);
        free(pipes[1]);
        return false;
    }

    ends[1] = FsCreateFileHandle(files[1], FILE_READ | FILE_WRITE);
    if (ends[1] == NULL)
    {
        FsReleaseFile(files[0]);
        FsReleaseFile(files[1]);
        pipes[1]->is_closed = true;
        HndClose(&ends[0]->hdr);
        return false;
    }

    return true;
}
