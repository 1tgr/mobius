#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/handle.h>
#include <kernel/thread.h>
#include <kernel/fs.h>

#define DEBUG
#include <kernel/debug.h>

#include <os/ioctl.h>
#include <wchar.h>
#include <errno.h>

#define PORT_BUFFER_SIZE    4084

typedef struct port_t port_t;
struct port_t
{
    handle_hdr_t hdr;
    file_t file;
    wchar_t *name;
    bool is_server;
    port_t *prev, *next;
    semaphore_t sem_connect;

    union
    {
        struct
        {
            port_t *waiting_first, *waiting_last;
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

port_t *server_first, *server_last;
semaphore_t sem_listen;
unsigned port_num_unnamed;

static port_t *PortFind(const wchar_t *name)
{
    port_t *port;
    
    for (port = server_first; port != NULL; port = port->next)
        if (_wcsicmp(port->name, name) == 0)
            return port;

    return NULL;
}

static void PortAddRef(port_t *port)
{
    port->hdr.copies++;
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

    port->hdr.copies--;
    if (port->hdr.copies == 0)
    {
        HndRemovePtrEntries(NULL, &port->hdr);
        
        /*if (!port->is_server)
            free(port->u.client.buffer);
        
        free(port->name);
        free(port);*/
        return 0;
    }
    else
        return port->hdr.copies;
}

static bool PortConnect(port_t *port, const wchar_t *name, status_t *result)
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

    server = PortFind(name);
    if (server == NULL)
    {
        *result = ENOTFOUND;
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

    HndSignalPtr(&server->hdr, true);

    /*
     * Server will now call PortAccept; on the first read, this client will wait to be 
     *  accepted.
     */
    return true;
}

static port_t *PortCreate(device_t *dev, const wchar_t *name, uint32_t flags)
{
    port_t *port;

    port = malloc(sizeof(port_t));
    if (port == NULL)
        return NULL;

    memset(port, 0, sizeof(port_t));
    HndInit(&port->hdr, 'file');
    port->file.fsd = dev;
    port->file.flags = flags;
    port->name = _wcsdup(name);
    SemInit(&port->sem_connect);
    return port;
}

static port_t *PortLockHandle(struct process_t *proc, handle_t hnd)
{
    handle_hdr_t *ptr;
    ptr = HndLock(proc, hnd, 'file');
    if (ptr == NULL)
        return NULL;
    else
        return (port_t*) (ptr - 1);
}

static void PortStartIo(void *param)
{
    device_t *dev;
    asyncio_t *io, *next;
    port_t *port, *remote;
    request_fs_t *req_fs;
    handle_t fd;
    size_t bytes;
    uint8_t *buf;
    struct process_t *proc;
    bool again;

    wprintf(L"PortStartIo(%p)\n", param);
    dev = param;
    do
    {
        again = false;
        for (io = dev->io_first; io != NULL; io = next)
        {
            next = io->next;

            assert(io->req->code == FS_READ || io->req->code == FS_WRITE);
            req_fs = (request_fs_t*) io->req;
            fd = req_fs->params.fs_read.file;
            proc = io->owner->process;
            port = PortLockHandle(proc, fd);
            assert(port != NULL);
            assert(!port->is_server);

            if (port->u.client.is_accepted)
            {
                remote = port->u.client.server;

                switch (req_fs->header.code)
                {
                case FS_READ:
                    if (port->u.client.readptr <= port->u.client.writeptr)
                        /* read, write, end */
                        bytes = port->u.client.writeptr - port->u.client.readptr;
                    else
                        /* write, read, end (write wrapped round) */
                        bytes = port->u.client.buffer_end - port->u.client.readptr;

                    if (bytes > req_fs->params.fs_read.length - io->length)
                        bytes = req_fs->params.fs_read.length - io->length;

                    wprintf(L"PortStartIo(FS_READ): port = %p(%s)=>%p(%s) readptr = %p writeptr = %p bytes = %u\n",
                        port, port->name, remote, remote->name,
                        port->u.client.readptr, port->u.client.writeptr, bytes);

                    buf = DevMapBuffer(io) + io->mod_buffer_start + io->length;
                    memcpy(buf, port->u.client.readptr, bytes);
                    io->length += bytes;
                    port->u.client.readptr += bytes;

                    assert(port->u.client.readptr <= port->u.client.buffer_end);
                    if (port->u.client.readptr == port->u.client.buffer_end)
                        port->u.client.readptr = port->u.client.buffer;

                    DevUnmapBuffer();

                    if (bytes > 0)
                        again = true;

                    if (io->length >= req_fs->params.fs_read.length)
                    {
                        wprintf(L"PortStartIo(FS_READ): finished\n");
                        DevFinishIo(dev, io, 0);
                    }
                    else
                        wprintf(L"PortStartIo(FS_READ): %u bytes left\n", req_fs->params.fs_read.length - io->length);

                    break;
                    
                case FS_WRITE:
                    if (remote->u.client.writeptr >= remote->u.client.readptr)
                        /* read, write, end */
                        bytes = remote->u.client.buffer_end - remote->u.client.writeptr;
                    else
                        /* write, read, end (write wrapped round) */
                        bytes = remote->u.client.readptr - remote->u.client.writeptr;

                    if (bytes > req_fs->params.fs_write.length - io->length)
                        bytes = req_fs->params.fs_write.length - io->length;

                    wprintf(L"PortStartIo(FS_WRITE): port = %p(%s)=>%p(%s) readptr = %p writeptr = %p bytes = %u\n",
                        port, port->name, remote, remote->name,
                        remote->u.client.readptr, remote->u.client.writeptr, bytes);

                    buf = DevMapBuffer(io) + io->mod_buffer_start + io->length;
                    memcpy(remote->u.client.writeptr, buf, bytes);
                    io->length += bytes;
                    remote->u.client.writeptr += bytes;

                    assert(remote->u.client.writeptr <= remote->u.client.buffer_end);
                    if (remote->u.client.writeptr == remote->u.client.buffer_end)
                        remote->u.client.writeptr = remote->u.client.buffer;

                    DevUnmapBuffer();

                    if (bytes > 0)
                        again = true;

                    if (io->length >= req_fs->params.fs_write.length)
                    {
                        wprintf(L"PortStartIo(FS_WRITE): finished\n");
                        DevFinishIo(dev, io, 0);
                    }
                    else
                        wprintf(L"PortStartIo(FS_WRITE): %u bytes left\n", req_fs->params.fs_write.length - io->length);

                    break;
                }
            }

            HndUnlock(proc, fd, 'file');
        }
    } while (false);
}

static void PortQueueStartIo(device_t *dev)
{
    ThrQueueKernelApc(current, PortStartIo, dev);
}

static bool PortDoPortRequest(device_t *dev, port_t *port, uint32_t code, request_fs_t *original, 
    params_port_t *params, status_t *result)
{
    port_t *client, *remote;
    
    switch (code)
    {
    case PORT_LISTEN:
        if (port->is_server)
        {
            *result = EINVALID;
            return false;
        }

        SemAcquire(&sem_listen);
        port->is_server = true;
        LIST_ADD(server, port);
        wprintf(L"PortDoPortRequest(PORT_LISTEN): listening on %p(%s)\n", port, port->name);
        SemRelease(&sem_listen);
        return true;

    case PORT_CONNECT:
        return PortConnect(port, params->port_connect.remote, result);

    case PORT_ACCEPT:
        if (!port->is_server)
        {
            wprintf(L"PortDoPortRequest(PORT_ACCEPT, %s): not a server\n", port->name);
            params->port_accept.client = NULL;
            *result = EINVALID;
            return false;
        }

        if (port->u.server.waiting_first == NULL)
        {
            wprintf(L"PortDoPortRequest(PORT_ACCEPT, %s): no clients waiting\n", port->name);
            params->port_accept.client = NULL;
            *result = ENOCLIENT;
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

        client = PortCreate(dev, L"", params->port_accept.flags);
        client->u.client.buffer = malloc(PORT_BUFFER_SIZE);
        client->u.client.buffer_end = client->u.client.buffer + PORT_BUFFER_SIZE;
        client->u.client.readptr = client->u.client.writeptr = client->u.client.buffer;
        remote->u.client.is_accepted = client->u.client.is_accepted = true;
        client->u.client.server = remote;
        remote->u.client.server = client;
        PortAddRef(client);
        PortAddRef(remote);

        SemRelease(&remote->sem_connect);

        params->port_accept.client = HndDuplicate(NULL, &client->hdr);
        PortQueueStartIo(dev);
        return true;
    }

    *result = ENOTIMPL;
    return false;
}

bool PortFsRequest(device_t *dev, request_t *req)
{
    request_fs_t *req_fs;
    port_t *port;
    wchar_t client_name[20];
    asyncio_t *io;
    
    req_fs = (request_fs_t*) req;
    switch (req->code)
    {
    case FS_CREATE:
        if (*req_fs->params.fs_create.name == '/')
            req_fs->params.fs_create.name++;
            
        port = PortCreate(dev, req_fs->params.fs_create.name, req_fs->params.fs_create.flags);
        if (port == NULL)
        {
            req_fs->params.fs_create.file = NULL;
            req->result = errno;
            return false;
        }
        else
        {
            TRACE2("PortFsRequest: created port %p(%s)\n", port, port->name);
            req_fs->params.fs_create.file = HndDuplicate(NULL, &port->hdr);
            return true;
        }
        
    case FS_OPEN:
        if (*req_fs->params.fs_open.name == '/')
            req_fs->params.fs_open.name++;

        swprintf(client_name, L"client_%u", ++port_num_unnamed);

        wprintf(L"PortFsRequest(FS_CREATE): client = %s, remote = %s\n", 
            client_name,
            req_fs->params.fs_open.name);
        port = PortCreate(dev, client_name, req_fs->params.fs_open.flags);
        if (port == NULL)
        {
            req_fs->params.fs_open.file = NULL;
            req->result = errno;
            return false;
        }

        if (PortConnect(port, req_fs->params.fs_open.name, &req->result))
        {
            TRACE2("PortFsRequest: connected %s to %s\n", port->name, port->u.client.server->name);
            req_fs->params.fs_open.file = HndDuplicate(NULL, &port->hdr);
            return true;
        }
        else
        {
            free(port->name);
            free(port);
            req_fs->params.fs_create.file = NULL;
            return false;
        }

    case FS_CLOSE:
        port = PortLockHandle(NULL, req_fs->params.fs_close.file);
        if (port == NULL)
        {
            req->result = EHANDLE;
            return false;
        }

        if (PortRelease(port) > 0)
            HndUnlock(NULL, req_fs->params.fs_close.file, 'file');
            
        return true;

    case FS_PASSTHROUGH:
        port = PortLockHandle(NULL, req_fs->params.fs_passthrough.file);
        if (port == NULL)
        {
            req->result = EHANDLE;
            return false;
        }
        
        if (PortDoPortRequest(dev, 
            port,
            req_fs->params.fs_passthrough.code,
            req_fs,
            req_fs->params.fs_passthrough.params,
            &req->result))
        {
            HndUnlock(NULL, req_fs->params.fs_passthrough.file, 'file');
            return true;
        }
        else
        {
            HndUnlock(NULL, req_fs->params.fs_passthrough.file, 'file');
            return false;
        }

    case FS_READ:
        port = PortLockHandle(NULL, req_fs->params.fs_read.file);
        if (port == NULL)
        {
            req->result = EHANDLE;
            return false;
        }

        if (port->is_server)
        {
            req->result = EINVALID;
            HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
            return false;
        }

        io = DevQueueRequest(dev, &req_fs->header, sizeof(request_fs_t),
            req_fs->params.fs_read.buffer,
            req_fs->params.fs_read.length);

        if (io == NULL)
        {
            req->result = errno;
            HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
            return false;
        }

        io->length = 0;
        HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
        PortQueueStartIo(dev);
        return true;

    case FS_WRITE:
        return true;

    case FS_IOCTL:
        port = PortLockHandle(NULL, req_fs->params.fs_read.file);
        if (port == NULL)
        {
            req->result = EHANDLE;
            return false;
        }

        switch (req_fs->params.fs_ioctl.code)
        {
        case IOCTL_BYTES_AVAILABLE:
            if (req_fs->params.fs_ioctl.length < sizeof(size_t))
            {
                HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
                req->result = EBUFFER;
                return false;
            }

            if (port->u.client.readptr <= port->u.client.writeptr)
                /* read, write, end */
                *(size_t*) req_fs->params.fs_ioctl.buffer = 
                    port->u.client.writeptr - port->u.client.readptr;
            else
                /* write, read, end (write wrapped round) */
                *(size_t*) req_fs->params.fs_ioctl.buffer = 
                    (port->u.client.buffer_end - port->u.client.readptr) + 
                        (port->u.client.writeptr - port->u.client.buffer);
            break;
        }

        HndUnlock(NULL, req_fs->params.fs_read.file, 'file');
        return true;
    }

    req->result = ENOTIMPL;
    return false;
}


device_t *PortMountFs(driver_t *driver, const wchar_t *name, device_t *dev);

driver_t port_driver =
{
    NULL,
    NULL,
    NULL,
    NULL,
    PortMountFs
};

static const device_vtbl_t portfs_vtbl =
{
    PortFsRequest,  /* request */
    NULL,           /* isr */
};

static device_t port_dev =
{
    &port_driver, 
    NULL,
    NULL, NULL,
    NULL,
    &portfs_vtbl,
};

device_t *PortMountFs(driver_t *driver, const wchar_t *name, device_t *dev)
{
    assert(driver == &port_driver);
    return &port_dev;
}
