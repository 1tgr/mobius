/* $Id: port.c,v 1.1 2002/12/21 09:49:27 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/fs.h>
#include <kernel/thread.h>

#include <errno.h>
#include <wchar.h>

extern struct module_t mod_kernel;
typedef struct port_t port_t;

typedef struct port_waiter_t port_waiter_t;
struct port_waiter_t
{
    port_waiter_t *prev, *next;
    pipe_t *end_server, *end_client;
};

struct port_t
{
    bool is_server;
    port_t *prev, *next;

    union
    {
        struct
        {
            spinlock_t sem;
            fs_asyncio_t *io_listen;
            page_array_t *pages_listen;
            wchar_t *name;
            port_waiter_t *waiter_first, *waiter_last;
        } server;

        pipe_t *client;
    } u;
};

typedef struct port_fsd_t port_fsd_t;
struct port_fsd_t
{
    fsd_t fsd;
    spinlock_t sem;
    port_t *server_first, *server_last;
};

extern fsd_t pipe_fsd;
bool PipeReadFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
                  size_t length, fs_asyncio_t *io);
bool PipeWriteFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
                   size_t length, fs_asyncio_t *io);
bool PipeIoCtlFile(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
                   size_t length, fs_asyncio_t *io);
void PipeFreeCookie(fsd_t *fsd, void *cookie);

static void PortWakeBlockedAcceptors(port_t *server)
{
    params_port_t *params;
    fs_asyncio_t *io;
    port_waiter_t *waiter;

    assert(server->is_server);
    SpinAcquire(&server->u.server.sem);
    io = server->u.server.io_listen;
    waiter = server->u.server.waiter_first;
    //wprintf(L"PortWakeBlockedAcceptors(%p): io = %p, waiter = %p\n",
        //server, io, waiter);

    if (io != NULL && waiter != NULL)
    {
        params = MemMapPageArray(server->u.server.pages_listen, 
            PRIV_WR | PRIV_KERN | PRIV_PRES);
        params->port_accept.client = 
            FsCreateFileHandle(io->owner->process, &pipe_fsd, waiter->end_server,
                NULL, 
                params->port_accept.flags);
        //wprintf(L"PortWakeBlockedAcceptors(%p): pipe = %p, handle = %u\n",
            //server, waiter->end_server, params->port_accept.client);
        MemUnmapTemp();

        LIST_REMOVE(server->u.server.waiter, waiter);
        free(waiter);
        MemDeletePageArray(server->u.server.pages_listen);
        server->u.server.io_listen = NULL;
        server->u.server.pages_listen = NULL;
        SpinRelease(&server->u.server.sem);
        FsNotifyCompletion(io, sizeof(params_port_t), 0);
    }
    else
        SpinRelease(&server->u.server.sem);
}

status_t PortParseElement(fsd_t *fsd, const wchar_t *name, 
                          wchar_t **new_path, vnode_t *node)
{
    port_fsd_t *pfsd;
    port_t *server;

    pfsd = (port_fsd_t*) fsd;
    assert(node->id == VNODE_ROOT);

    SpinAcquire(&pfsd->sem);
    for (server = pfsd->server_first; server != NULL; server = server->next)
    {
        assert(server->is_server);
        if (_wcsicmp(server->u.server.name, name) == 0)
        {
            node->id = (vnode_id_t) server;
            SpinRelease(&pfsd->sem);
            return 0;
        }
    }

    SpinRelease(&pfsd->sem);
    return ENOTFOUND;
}

status_t PortCreateFile(fsd_t *fsd, vnode_id_t dir, const wchar_t *name, 
                        void **cookie)
{
    port_fsd_t *pfsd;
    port_t *server;

    pfsd = (port_fsd_t*) fsd;
    assert(dir == VNODE_ROOT);

    //wprintf(L"PortCreateFile(%s)\n", name);

    server = malloc(sizeof(port_t));
    if (server == NULL)
        return errno;

    memset(server, 0, sizeof(port_t));
    server->is_server = true;
    server->u.server.name = _wcsdup(name);
    *cookie = server;

    SpinAcquire(&pfsd->sem);
    LIST_ADD(pfsd->server, server);
    SpinRelease(&pfsd->sem);

    return 0;
}

status_t PortLookupFile(fsd_t *fsd, vnode_id_t node, uint32_t open_flags, 
                        void **cookie)
{
    port_fsd_t *pfsd;
    port_t *server, *client;
    port_waiter_t *waiter;
    pipe_t *ends[2];

    pfsd = (port_fsd_t*) fsd;
    if (node == VNODE_ROOT)
        return EACCESS;

    server = (port_t*) node;
    assert(server->is_server);

    client = malloc(sizeof(port_t));
    if (client == NULL)
        goto error0;

    waiter = malloc(sizeof(port_waiter_t));
    if (waiter == NULL)
        goto error1;

    if (!FsCreatePipeInternal(ends))
        goto error2;

    client->is_server = false;
    waiter->end_client = client->u.client = ends[0];
    waiter->end_server = ends[1];

    SpinAcquire(&server->u.server.sem);
    LIST_ADD(server->u.server.waiter, waiter);
    SpinRelease(&server->u.server.sem);

    *cookie = client;
    PortWakeBlockedAcceptors(server);
    return 0;

error2:
    free(waiter);
error1:
    free(client);
error0:
    return errno;
}

void PortFreeCookie(fsd_t *fsd, void *cookie)
{
    port_fsd_t *pfsd;
    port_t *server;

    pfsd = (port_fsd_t*) fsd;
    server = cookie;

    if (server->is_server)
    {
        SpinAcquire(&pfsd->sem);
        LIST_REMOVE(pfsd->server, server);
        SpinRelease(&pfsd->sem);

        free(server->u.server.name);
    }
    else
        PipeFreeCookie(fsd, server->u.client);

    free(server);
}

bool PortReadFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
                  size_t length, fs_asyncio_t *io)
{
    port_fsd_t *pfsd;
    port_t *port;

    pfsd = (port_fsd_t*) fsd;
    port = file->fsd_cookie;
    if (port->is_server)
    {
        io->op.result = EACCESS;
        return false;
    }
    else
    {
        file_t temp;
        temp.fsd_cookie = port->u.client;
        return PipeReadFile(fsd, &temp, pages, length, io);
    }
}

bool PortWriteFile(fsd_t *fsd, file_t *file, page_array_t *pages, 
                   size_t length, fs_asyncio_t *io)
{
    port_fsd_t *pfsd;
    port_t *port;

    pfsd = (port_fsd_t*) fsd;
    port = file->fsd_cookie;
    if (port->is_server)
    {
        io->op.result = EACCESS;
        return false;
    }
    else
    {
        file_t temp;
        temp.fsd_cookie = port->u.client;
        return PipeWriteFile(fsd, &temp, pages, length, io);
    }
}

bool PortIoCtlFile(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
                   size_t length, fs_asyncio_t *io)
{
    port_fsd_t *pfsd;
    port_t *port;

    pfsd = (port_fsd_t*) fsd;
    port = file->fsd_cookie;
    if (port->is_server)
    {
        io->op.result = EACCESS;
        return false;
    }
    else
    {
        file_t temp;
        temp.fsd_cookie = port->u.client;
        return PipeIoCtlFile(fsd, &temp, code, buf, length, io);
    }
}

bool PortPassthrough(fsd_t *fsd, file_t *file, uint32_t code, void *buf, 
                     size_t length, fs_asyncio_t *io)
{
    params_port_t *params;
    port_t *server;

    params = buf;

    if (length < sizeof(params_port_t))
    {
        io->op.result = EBUFFER;
        return false;
    }

    server = file->fsd_cookie;
    if (!server->is_server)
    {
        io->op.result = EACCESS;
        return false;
    }

    switch (code)
    {
    case PORT_LISTEN:
    case PORT_CONNECT:
        //wprintf(L"PortPassthrough(%p): PORT_LISTEN and PORT_CONNECT not implemented\n",
            //server);
        //FsNotifyCompletion(io, length, 0);
        //break;
        io->op.result = ENOTIMPL;
        return false;

    case PORT_ACCEPT:
        SpinAcquire(&server->u.server.sem);
        if (server->u.server.io_listen != NULL)
        {
            SpinRelease(&server->u.server.sem);
            io->op.result = EACCESS;
            return false;
        }

        //wprintf(L"PortPassthrough(%p): PORT_ACCEPT, waiter = %p\n",
            //server,
            //server->u.server.waiter_first);
        server->u.server.io_listen = io;
        server->u.server.pages_listen = MemCreatePageArray(buf, length);
        SpinRelease(&server->u.server.sem);
        io->op.result = io->original->result = SIOPENDING;
        PortWakeBlockedAcceptors(server);
        break;

    default:
        io->op.result = ENOTIMPL;
        return false;
    }

    return true;
}

status_t PortOpenDir(fsd_t *fsd, vnode_id_t node, void **dir_cookie)
{
    port_fsd_t *pfsd;
    port_t **server;

    assert(node == VNODE_ROOT);
    pfsd = (port_fsd_t*) fsd;

    server = malloc(sizeof(port_t*));
    *dir_cookie = server;
    if (*dir_cookie == NULL)
        return errno;

    *server = pfsd->server_first;
    return 0;
}

status_t PortReadDir(fsd_t *fsd, void *dir_cookie, dirent_t *buf)
{
    port_fsd_t *pfsd;
    port_t **server;

    pfsd = (port_fsd_t*) fsd;
    server = dir_cookie;

    if (*server == NULL)
        return EEOF;

    assert((*server)->is_server);
    buf->vnode = 0;
    wcsncpy(buf->name, (*server)->u.server.name, _countof(buf->name));
    *server = (*server)->next;
    return 0;
}

void PortFreeDirCookie(fsd_t *fsd, void *dir_cookie)
{
    free(dir_cookie);
}

static vtbl_fsd_t port_vtbl =
{
    NULL,           /* dismount */
    NULL,           /* get_fs_info */
    PortParseElement,
    PortCreateFile,
    PortLookupFile,
    NULL,           /* get_file_info */
    NULL,           /* set_file_info */
    PortFreeCookie,
    PortReadFile,
    PortWriteFile,
    PortIoCtlFile,
    PortPassthrough,
    NULL,           /* mkdir */
    PortOpenDir,
    PortReadDir,
    PortFreeDirCookie,
    NULL,           /* finishio */
    NULL,           /* flush_cache */
};

fsd_t *PortMountFs(driver_t *driver, const wchar_t *dest)
{
    port_fsd_t *pfsd;

    pfsd = malloc(sizeof(port_fsd_t));
    if (pfsd == NULL)
        return NULL;

    pfsd->fsd.vtbl = &port_vtbl;
    SpinInit(&pfsd->sem);
    pfsd->server_first = pfsd->server_last = NULL;

    return &pfsd->fsd;
}

driver_t port_driver =
{
    &mod_kernel,
    NULL, NULL,
    NULL,
    NULL,
    PortMountFs,
};
