/* $Id: port.c,v 1.12 2002/05/05 13:43:24 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/handle.h>
#include <kernel/thread.h>
#include <kernel/fs.h>
#include <kernel/init.h>

#define DEBUG
#include <kernel/debug.h>

#include <os/ioctl.h>
#include <wchar.h>
#include <errno.h>

#define PORT_BUFFER_SIZE    4084

typedef struct port_t port_t;
struct port_t
{
    wchar_t *name;
    bool is_server;
    port_t *prev, *next;
    semaphore_t sem_connect;
    unsigned copies;

    union
    {
        struct
        {
            port_t *waiting_first, *waiting_last;
            fs_asyncio_t *listener;
        } server;

        struct
        {
            port_t *server;
            bool is_accepted;
            uint8_t *buffer, *buffer_end;
            uint8_t *writeptr, *readptr;
        } client;
    } u;
};

typedef struct port_asyncio_t port_asyncio_t;
struct port_asyncio_t
{
    port_asyncio_t *prev, *next;
    file_t *file;
    page_array_t *pages;
    size_t total_length;
    size_t length;
    fs_asyncio_t *io;
    bool is_reading;
};

typedef struct port_dir_t port_dir_t;
struct port_dir_t
{
    fsd_t fsd;
    port_t *server_first, *server_last;
    semaphore_t sem_listen;
    unsigned port_num_unnamed;
    port_asyncio_t *io_first, *io_last;
};

static port_t *PortFind(port_dir_t *dir, const wchar_t *name)
{
    port_t *port;
    
    for (port = dir->server_first; port != NULL; port = port->next)
        if (_wcsicmp(port->name, name) == 0)
            return port;

    return NULL;
}

static void PortAddRef(port_t *port)
{
    port->copies++;
}

static int PortRelease(port_t *port)
{
    /*if (port->is_server)
    {
        LIST_REMOVE(server, port);
    }
    else if (port->u.client.server != NULL)
    {
        port_t *s;
        s = port->u.client.server;
        port->u.client.server = NULL;
        PortRelease(s);
    }*/

    port->copies--;
    if (port->copies == 0)
    {
        /*HndRemovePtrEntries(NULL, &port->hdr);*/
        
        /*if (!port->is_server)
            free(port->u.client.buffer);
        
        free(port->name);
        free(port);*/
        return 0;
    }
    else
        return port->copies;
}

static bool PortConnect(port_t *port, port_dir_t *dir, const wchar_t *name, status_t *result)
{
    port_t *server;

    /*
     * Connect a client port to a server.
     * Place the client in the server's 'waiting' queue and block until the server does an 
     *  accept.
     */
    if (port->is_server)
    {
        *result = EINVALID;
        return false;
    }

    server = PortFind(dir, name);
    if (server == NULL)
    {
        *result = ENOTFOUND;
        return false;
    }

    if (server->u.server.listener == NULL)
    {
        wprintf(L"PortConnect(%s=>%s): server is already handling another connection\n",
            port->name, server->name);
        *result = EACCESS;
        return false;
    }

    SemAcquire(&server->sem_connect);
    SemAcquire(&port->sem_connect);
    port->u.client.server = server;
    LIST_ADD(server->u.server.waiting, port);
    port->u.client.buffer = malloc(PORT_BUFFER_SIZE);
    port->u.client.buffer_end = port->u.client.buffer + PORT_BUFFER_SIZE;
    port->u.client.readptr = port->u.client.writeptr = port->u.client.buffer;
    PortAddRef(port);
    PortAddRef(server);
    SemRelease(&port->sem_connect);
    SemRelease(&server->sem_connect);

    FsNotifyCompletion(server->u.server.listener, 0, 0);
    server->u.server.listener = NULL;

    /*HndSignalPtr(&server->hdr, true);*/

    /*
     * Server will now call PortAccept; on the first read, this client will wait to be 
     *  accepted.
     */
    return true;
}

static port_t *PortCreate(const wchar_t *name)
{
    port_t *port;

    port = malloc(sizeof(port_t));
    if (port == NULL)
        return NULL;

    memset(port, 0, sizeof(port_t));
    port->name = _wcsdup(name);
    SemInit(&port->sem_connect);
    return port;
}

/*static port_t *PortLockHandle(struct process_t *proc, handle_t hnd)
{
    handle_hdr_t *ptr;
    ptr = HndLock(proc, hnd, 'file');
    if (ptr == NULL)
        return NULL;
    else
        return (port_t*) (ptr - 1);
}*/

void PortFinishIo(port_dir_t *dir, port_asyncio_t *io, size_t bytes, status_t result)
{
    FsNotifyCompletion(io->io, bytes, result);
    LIST_REMOVE(dir->io, io);
    free(io);
}

static void PortStartIo(void *param)
{
    port_dir_t *dir;
    port_asyncio_t *io, *next;
    port_t *port, *remote;
    size_t bytes;
    uint8_t *buf;
    bool again;

    wprintf(L"PortStartIo(%p): started\n", param);
    dir = param;
    do
    {
        again = false;
        for (io = dir->io_first; io != NULL; io = next)
        {
            next = io->next;

            /*fd = req_fs->params.fs_read.file;
            proc = io->owner->process;
            port = PortLockHandle(proc, fd);
            assert(port != NULL);*/
            port = io->file->fsd_cookie;
            assert(!port->is_server);

            if (port->u.client.is_accepted)
            {
                remote = port->u.client.server;

                if (io->is_reading)
                {
                    if (port->u.client.readptr <= port->u.client.writeptr)
                        /* read, write, end */
                        bytes = port->u.client.writeptr - port->u.client.readptr;
                    else
                        /* write, read, end (write wrapped round) */
                        bytes = port->u.client.buffer_end - port->u.client.readptr;

                    if (bytes > io->total_length - io->length)
                        bytes = io->total_length - io->length;

                    wprintf(L"PortStartIo(read): port = %p(%s)=>%p(%s) readptr = %p writeptr = %p bytes = %u\n",
                        port, port->name, remote, remote->name,
                        port->u.client.readptr, port->u.client.writeptr, bytes);

                    //buf = DevMapBuffer(io) + io->mod_buffer_start + io->length;
                    buf = MemMapPageArray(io->pages, 
                        PRIV_WR | PRIV_KERN | PRIV_PRES);
                    memcpy(buf + io->length, port->u.client.readptr, bytes);
                    io->length += bytes;
                    port->u.client.readptr += bytes;

                    assert(port->u.client.readptr <= port->u.client.buffer_end);
                    if (port->u.client.readptr == port->u.client.buffer_end)
                        port->u.client.readptr = port->u.client.buffer;

                    MemUnmapTemp();

                    if (bytes > 0)
                        again = true;

                    if (io->length >= io->total_length)
                    {
                        wprintf(L"PortStartIo(read): finished read\n");
                        PortFinishIo(dir, io, io->length, 0);
                        /*DevFinishIo(dev, io, 0);*/
                    }
                    else
                        wprintf(L"PortStartIo(read): %u bytes left\n", io->total_length - io->length);
                }
                else
                {
                    if (remote->u.client.writeptr >= remote->u.client.readptr)
                        /* read, write, end */
                        bytes = remote->u.client.buffer_end - remote->u.client.writeptr;
                    else
                        /* write, read, end (write wrapped round) */
                        bytes = remote->u.client.readptr - remote->u.client.writeptr;

                    if (bytes > io->total_length - io->length)
                        bytes = io->total_length - io->length;

                    wprintf(L"PortStartIo(write): port = %p(%s)=>%p(%s) readptr = %p writeptr = %p bytes = %u\n",
                        port, port->name, remote, remote->name,
                        remote->u.client.readptr, remote->u.client.writeptr, bytes);

                    //buf = DevMapBuffer(io) + io->mod_buffer_start + io->length;
                    buf = MemMapPageArray(io->pages, 
                        PRIV_KERN | PRIV_PRES);
                    memcpy(remote->u.client.writeptr, buf + io->length, bytes);
                    io->length += bytes;
                    remote->u.client.writeptr += bytes;

                    assert(remote->u.client.writeptr <= remote->u.client.buffer_end);
                    if (remote->u.client.writeptr == remote->u.client.buffer_end)
                        remote->u.client.writeptr = remote->u.client.buffer;

                    MemUnmapTemp();

                    if (bytes > 0)
                        again = true;

                    if (io->length >= io->total_length)
                    {
                        wprintf(L"PortStartIo(write): finished write\n");
                        PortFinishIo(dir, io, io->length, 0);
                        /*DevFinishIo(dev, io, 0);*/
                    }
                    else
                        wprintf(L"PortStartIo(write): %u bytes left\n", io->total_length - io->length);
                }
            }
        }
    } while (again);

    wprintf(L"PortStartIo(%p): finished\n", param);
}

static void PortQueueStartIo(port_dir_t *dir)
{
    /*ThrQueueKernelApc(current, PortStartIo, dir);*/
    PortStartIo(dir);
}

void PortDismount(fsd_t *fsd)
{
    port_dir_t *dir;
    dir = (port_dir_t*) fsd;
    assert(dir->server_first == NULL);
    free(dir);
}

status_t PortCreateFile(fsd_t *fsd, const wchar_t *path, 
                        fsd_t **redirect, void **cookie)
{
    port_t *port;

    if (*path == '/')
        path++;
            
    port = PortCreate(path);
    if (port == NULL)
        return errno;

    TRACE2("PortCreateFile: created port %p(%s)\n", port, port->name);
    *cookie = port;
    return 0;
}

status_t PortLookupFile(fsd_t *fsd, const wchar_t *path, 
                        fsd_t **redirect, void **cookie)
{
    wchar_t client_name[20];
    status_t ret;
    port_dir_t *dir;
    port_t *port;

    dir = (port_dir_t*) fsd;
    if (*path == '/')
        path++;

    swprintf(client_name, L"client_%u", ++dir->port_num_unnamed);

    wprintf(L"PortLookupFile: client = %s, remote = %s\n", client_name, path);
    port = PortCreate(client_name);
    if (port == NULL)
        return errno;

    if (PortConnect(port, dir, path, &ret))
    {
        TRACE2("PortLookupFile: connected %s to %s\n", port->name, port->u.client.server->name);
        *cookie = port;
        return 0;
    }
    else
    {
        free(port->name);
        free(port);
        return ret;
    }
}

status_t PortGetFileInfo(fsd_t *fsd, void *cookie, uint32_t type, void *buf)
{
    return ENOTIMPL;
}

void PortFreeCookie(fsd_t *fsd, void *cookie)
{
    PortRelease(cookie);
}


bool PortQueueRequest(port_dir_t *dir, file_t *file, page_array_t *pages,
                      size_t length, fs_asyncio_t *io, bool is_reading)
{
    port_asyncio_t *pio;

    pio = malloc(sizeof(port_asyncio_t));
    if (pio == NULL)
        return false;
    
    pio->file = file;
    pio->pages = MemCopyPageArray(pages->num_pages, pages->mod_first_page, 
        pages->pages);

    if (pio->pages == NULL)
    {
        free(pio);
        return false;
    }

    pio->total_length = length;
    pio->length = 0;
    pio->io = io;
    pio->is_reading = is_reading;
    LIST_ADD(dir->io, pio);
    return true;
}

bool PortReadWriteFile(fsd_t *fsd, file_t *file, page_array_t *pages,
                       size_t length, fs_asyncio_t *io, bool is_reading)
{
    port_t *port;
    port_dir_t *dir;

    port = file->fsd_cookie;
    if (port->is_server)
    {
        io->op.result = EINVALID;
        return false;
    }

    dir = (port_dir_t*) fsd;

    /*array = MemCreatePageArray(req_fs->params.fs_read.buffer, 
        req_fs->params.fs_read.length);
    if (array == NULL)
    {
        req->result = errno;
        HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
        return false;
    }*/

    /*io = DevQueueRequest(dev, &req_fs->header, sizeof(request_fs_t),
        req_fs->params.fs_read.pages,
        req_fs->params.fs_read.length);*/

    /*MemDeletePageArray(array);*/

    wprintf(L"PortReadWriteFile(%s): queueing request\n", 
        is_reading ? L"read" : L"write");
    if (!PortQueueRequest(dir, file, pages, length, io, is_reading))
    {
        io->op.result = errno;
        return false;
    }

    io->op.result = io->original->result = SIOPENDING;
    PortQueueStartIo(dir);
    return true;
}

bool PortReadFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
                  size_t length, fs_asyncio_t *io)
{
    return PortReadWriteFile(fsd, file, pages, length, io, true);
}

bool PortWriteFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
                   size_t length, fs_asyncio_t *io)
{
    return PortReadWriteFile(fsd, file, pages, length, io, false);
}

bool PortIoCtlFile(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
                   size_t length, fs_asyncio_t *io)
{
    port_t *port;

    port = file->fsd_cookie;
    io->op.result = 0;
    switch (code)
    {
    case IOCTL_BYTES_AVAILABLE:
        if (length < sizeof(size_t))
        {
            io->op.result = EBUFFER;
            return false;
        }

        if (port->u.client.readptr <= port->u.client.writeptr)
            /* read, write, end */
            *(size_t*) buf = 
                port->u.client.writeptr - port->u.client.readptr;
        else
            /* write, read, end (write wrapped round) */
            *(size_t*) buf = 
                (port->u.client.buffer_end - port->u.client.readptr) + 
                    (port->u.client.writeptr - port->u.client.buffer);

        return true;
    }

    io->op.result = ENOTIMPL;
    return false;
}

bool PortPassthrough(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
                     size_t length, fs_asyncio_t *io)
{
    port_dir_t *dir;
    port_t *port, *client, *remote;
    params_port_t *params;

    dir = (port_dir_t*) fsd;
    port = file->fsd_cookie;
    params = (params_port_t*) buf;

    if (length < sizeof(params_port_t) ||
        !MemVerifyBuffer(params, length))
    {
        io->op.result = EBUFFER;
        return false;
    }

    switch (code)
    {
    case PORT_LISTEN:
        if (port->is_server && 
            port->u.server.listener != NULL)
        {
            io->op.result = EINVALID;
            return false;
        }

        SemAcquire(&dir->sem_listen);

        if (!port->is_server)
            LIST_ADD(dir->server, port);

        port->is_server = true;
        port->u.server.listener = io;
        wprintf(L"PortDoPortRequest(PORT_LISTEN): listening on %p(%s)\n", port, port->name);
        SemRelease(&dir->sem_listen);
        io->op.result = io->original->result = SIOPENDING;
        return true;

    case PORT_CONNECT:
        return PortConnect(port, dir, params->port_connect.remote, &io->op.result);

    case PORT_ACCEPT:
        if (!port->is_server)
        {
            wprintf(L"PortDoPortRequest(PORT_ACCEPT, %s): not a server\n", port->name);
            params->port_accept.client = NULL;
            io->op.result = EINVALID;
            return false;
        }

        if (port->u.server.waiting_first == NULL)
        {
            wprintf(L"PortDoPortRequest(PORT_ACCEPT, %s): no clients waiting\n", port->name);
            params->port_accept.client = NULL;
            io->op.result = ENOCLIENT;
            return false;
        }

        remote = port->u.server.waiting_first;
        wprintf(L"PortDoPortRequest(PORT_ACCEPT): accepting %s on %s\n",
            remote->name, port->name);
        SemAcquire(&remote->sem_connect);
        SemAcquire(&port->sem_connect);
        LIST_REMOVE(port->u.server.waiting, remote);
        PortRelease(port);
        SemRelease(&port->sem_connect);

        client = PortCreate(L"");
        client->u.client.buffer = malloc(PORT_BUFFER_SIZE);
        client->u.client.buffer_end = client->u.client.buffer + PORT_BUFFER_SIZE;
        client->u.client.readptr = client->u.client.writeptr = client->u.client.buffer;
        remote->u.client.is_accepted = client->u.client.is_accepted = true;
        client->u.client.server = remote;
        remote->u.client.server = client;
        PortAddRef(client);
        PortAddRef(remote);

        SemRelease(&remote->sem_connect);

        params->port_accept.client = 
            FsCreateFileHandle(NULL, fsd, client, NULL, params->port_accept.flags);
        PortQueueStartIo(dir);
        return true;
    }

    io->op.result = ENOTIMPL;
    return false;
}

status_t PortOpenDir(fsd_t *fsd, const wchar_t *path, fsd_t **redirect, void **dir_cookie)
{
    return ENOTIMPL;
}

status_t PortReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
    return ENOTIMPL;
}

void PortFreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
}

static const fsd_vtbl_t portfs_vtbl =
{
    PortDismount,       /* dismount */
    NULL,               /* get_fs_info */
    PortCreateFile,
    PortLookupFile,
    PortGetFileInfo,
    NULL,               /* set_file_info */
    PortFreeCookie,
    PortReadFile,
    PortWriteFile,
    PortIoCtlFile,
    PortPassthrough,    /* passthrough */
    PortOpenDir,
    PortReadDir,
    PortFreeDirCookie,  /* free_dir_cookie */
    NULL,               /* mount */
    NULL,               /* finishio */
    NULL,               /* flush_cache */
};

fsd_t *PortMountFs(driver_t *driver, const wchar_t *dest)
{
    port_dir_t *dir;
    dir = malloc(sizeof(port_dir_t));
    memset(dir, 0, sizeof(port_dir_t));
    dir->fsd.vtbl = &portfs_vtbl;
    return &dir->fsd;
}

extern struct module_t mod_kernel;

driver_t port_driver =
{
    &mod_kernel,
    NULL,
    NULL,
    NULL,
    PortMountFs,
};
