/* $Id: pipe.c,v 1.1 2002/08/29 14:10:00 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <os/queue.h>
#include <errno.h>
#include <stdio.h>

typedef struct pipe_asyncio_t pipe_asyncio_t;

typedef struct pipe_t pipe_t;
struct pipe_t
{
    pipe_t *other;
    queue_t queue;
    pipe_asyncio_t *aio_first, *aio_last;
    spinlock_t sem_aio;
};

struct pipe_asyncio_t
{
    pipe_asyncio_t *prev, *next;
    file_t *file;
    page_array_t *pages;
    size_t length;
    fs_asyncio_t *io;
};

static void PipeWakeBlockedReaders(pipe_t *pipe)
{
    uint8_t *dest;
    pipe_asyncio_t *io, *next;

    wprintf(L"PipeWakeBlockedReaders(%p): %u bytes\n", pipe, pipe->queue.length);

    SpinAcquire(&pipe->sem_aio);
    for (io = pipe->aio_first; io != NULL; io = next)
    {
        next = io->next;
        if (pipe->queue.length >= io->length)
        {
            LIST_REMOVE(pipe->aio, io);
            wprintf(L"Pipe has %u bytes, request %p needs %u bytes\n",
                pipe->queue.length, io, io->length);
            dest = MemMapPageArray(io->pages, io->length);
            QueuePopFirst(&pipe->queue, dest, io->length);
            MemUnmapTemp();
            FsNotifyCompletion(io->io, io->length, 0);
            free(io);
        }
    }

    SpinRelease(&pipe->sem_aio);
}

void PipeFreeCookie(fsd_t *pipe, void *cookie)
{
}

bool PipeReadFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
    size_t length, fs_asyncio_t *io)
{
    pipe_t *pipe;
    pipe_asyncio_t *aio;

    pipe = file->fsd_cookie;

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

    aio->file = file;
    aio->pages = pages;
    aio->length = length;
    aio->io = io;

    SpinAcquire(&pipe->sem_aio);
    LIST_ADD(pipe->aio, aio);
    SpinRelease(&pipe->sem_aio);

    io->op.result = io->original->result = SIOPENDING;

    wprintf(L"PipeReadFile(%p)\n", pipe);
    PipeWakeBlockedReaders(pipe);
    return true;
}

bool PipeWriteFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
    size_t length, fs_asyncio_t *io)
{
    pipe_t *pipe;
    void *buf;

    pipe = file->fsd_cookie;
    buf = MemMapPageArray(pages, length);
    QueueAppend(&pipe->other->queue, buf, length);
    MemUnmapTemp();
    FsNotifyCompletion(io, length, 0);

    wprintf(L"PipeWriteFile(%p)\n", pipe);
    PipeWakeBlockedReaders(pipe->other);
    return true;
}

bool PipeIoCtlFile(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
    size_t length, fs_asyncio_t *io)
{
    io->op.result = ENOTIMPL;
    return false;
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
    NULL,           /* free_dir_cookie */
    NULL,           /* finishio */
    NULL,           /* flush_cache */
};

static fsd_t pipe_fsd = 
{
    &pipe_vtbl,
};

bool FsCreatePipe(handle_t *ends)
{
    pipe_t *pipes[2];

    pipes[0] = malloc(sizeof(pipe_t));
    if (pipes[0] == NULL)
        return false;

    memset(pipes[0], 0, sizeof(pipe_t));

    pipes[1] = malloc(sizeof(pipe_t));
    if (pipes[1] == NULL)
        goto error1;

    memset(pipes[1], 0, sizeof(pipe_t));

    QueueInit(&pipes[0]->queue);
    pipes[0]->other = pipes[1];
    QueueInit(&pipes[1]->queue);
    pipes[1]->other = pipes[0];

    ends[0] = FsCreateFileHandle(NULL, &pipe_fsd, pipes[0], NULL, 
        FILE_READ | FILE_WRITE);
    if (ends[0] == NULL)
        goto error2;

    ends[1] = FsCreateFileHandle(NULL, &pipe_fsd, pipes[1], NULL, 
        FILE_READ | FILE_WRITE);
    if (ends[1] == NULL)
    {
        HndClose(NULL, ends[0], 'file');
        goto error2;
    }

    return true;

error2:
    QueueClear(&pipes[1]->queue);
    free(pipes[1]);
error1:
    QueueClear(&pipes[0]->queue);
    free(pipes[0]);
    return false;
}
