/* $Id: pipe.c,v 1.3 2002/09/01 16:16:32 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/fs.h>

#include <os/ioctl.h>

#include <errno.h>
#include <stdio.h>

typedef struct pipe_asyncio_t pipe_asyncio_t;
struct pipe_asyncio_t
{
    pipe_asyncio_t *prev, *next;
    page_array_t *pages;
    size_t length;
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
    pipe_asyncio_t *io, *next;
    size_t length;

    length = pipe->write_ptr - pipe->read_ptr;
    //wprintf(L"PipeWakeBlockedReaders(%p): %u bytes\n", pipe, length);

    SpinAcquire(&pipe->sem);
    for (io = pipe->aio_first; io != NULL; io = next)
    {
        next = io->next;
        //if (io->length <= pipe->write_ptr - pipe->read_ptr)
        if (length > 0)
        {
            LIST_REMOVE(pipe->aio, io);
            length = pipe->write_ptr - pipe->read_ptr;
            if (length > io->length)
                length = io->length;
            //wprintf(L"Pipe has %u bytes, request %p needs %u bytes\n",
                //length, io, io->length);
            dest = MemMapPageArray(io->pages, io->length);
            memcpy(dest, pipe->buffer + pipe->read_ptr, length);
            MemUnmapTemp();
            pipe->read_ptr += length;
            FsNotifyCompletion(io->io, length, 0);

            MemDeletePageArray(io->pages);
            free(io);
        }
        else if (pipe->other->is_closed)
        {
            FsNotifyCompletion(io->io, 0, EPIPECLOSED);
            free(io);
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

bool PipeReadFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
    size_t length, fs_asyncio_t *io)
{
    pipe_t *pipe;
    pipe_asyncio_t *aio;

    /*
     * Don't use any fields in file other than fsd_cookie, because it might
     *  come from PortReadFile, which doesn't bother filling in the other 
     *  fields.
     */
    pipe = file->fsd_cookie;
    assert(!pipe->is_closed);

    if (pipe->aio_first != NULL)
    {
        /* xxx -- only allow one reader for now */
        io->op.result = EACCESS;
        return false;
    }

    aio = malloc(sizeof(pipe_asyncio_t));
    if (aio == NULL)
    {
        io->op.result = errno;
        return false;
    }

    //aio->file = file;
    aio->pages = MemCopyPageArray(pages);
    aio->length = length;
    aio->io = io;

    SpinAcquire(&pipe->sem);
    LIST_ADD(pipe->aio, aio);
    SpinRelease(&pipe->sem);

    io->op.result = io->original->result = SIOPENDING;

    //wprintf(L"PipeReadFile(%p)\n", pipe);
    PipeWakeBlockedReaders(pipe);
    return true;
}

bool PipeWriteFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
    size_t length, fs_asyncio_t *io)
{
    pipe_t *pipe, *other;
    void *buf;

    /*
     * Same proviso applies here as in PipeReadFile about the other fields in
     *  file.
     */
    pipe = file->fsd_cookie;
    assert(!pipe->is_closed);
    other = pipe->other;

    if (other->is_closed)
    {
        io->op.result = EPIPECLOSED;
        return false;
    }

    SpinAcquire(&pipe->other->sem);
    if (other->write_ptr + length > other->allocated)
    {
        /*wprintf(L"PipeWriteFile(%p): read = %u, write = %u, length = %u, allocated = %u\n",
            pipe,
            other->read_ptr,
            other->write_ptr,
            length,
            other->allocated);*/

        if (other->read_ptr == other->write_ptr)
            other->read_ptr = other->write_ptr = 0;

        if (other->write_ptr + length > other->allocated)
        {
            other->allocated = (other->write_ptr + length + 15) & -16;
            other->buffer = realloc(other->buffer, other->allocated);
            wprintf(L"PipeWriteFile(%p): reallocated to %u bytes\n", 
                pipe, other->allocated);
        }
    }

    assert(other->write_ptr + length <= other->allocated);

    buf = MemMapPageArray(pages, length);
    memcpy(other->buffer + other->write_ptr, buf, length);
    MemUnmapTemp();

    other->write_ptr += length;
    SpinRelease(&other->sem);
    FsNotifyCompletion(io, length, 0);

    PipeWakeBlockedReaders(other);
    return true;
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
    PipeReadFile,
    PipeWriteFile,
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

bool FsCreatePipe(handle_t *ends)
{
    pipe_t *pipes[2];

    if (!FsCreatePipeInternal(pipes))
        return false;

    ends[0] = FsCreateFileHandle(NULL, &pipe_fsd, pipes[0], NULL, 
        FILE_READ | FILE_WRITE);
    if (ends[0] == NULL)
    {
        free(pipes[0]);
        free(pipes[1]);
        return false;
    }

    ends[1] = FsCreateFileHandle(NULL, &pipe_fsd, pipes[1], NULL, 
        FILE_READ | FILE_WRITE);
    if (ends[1] == NULL)
    {
        pipes[1]->is_closed = true;
        HndClose(NULL, ends[0], 'file');
        return false;
    }

    return true;
}
