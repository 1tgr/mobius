/* $Id: port.c,v 1.4 2002/03/27 22:06:32 pavlovskii Exp $ */

#include <errno.h>
#include <wchar.h>

#include <os/syscall.h>
#include <os/device.h>
#include <os/defs.h>

bool PortListen(handle_t port)
{
    fileop_t op;
    params_port_t params;
    params.port_listen.port = port;
    if (FsRequestSync(port, PORT_LISTEN, &params, sizeof(params), &op))
        return true;
    else
    {
        errno = op.result;
        return false;
    }
}

bool PortConnect(handle_t port, const wchar_t *remote)
{
    fileop_t op;
    params_port_t params;
    params.port_connect.port = port;
    params.port_connect.remote = remote;
    params.port_connect.name_size = (wcslen(remote) + 1) * sizeof(wchar_t);
    if (FsRequestSync(port, PORT_CONNECT, &params, sizeof(params), &op))
        return true;
    else
    {
        errno = op.result;
        return false;
    }
}

handle_t PortAccept(handle_t port, uint32_t flags)
{
    fileop_t op;
    params_port_t params;
    params.port_accept.port = port;
    params.port_accept.flags = flags;
    if (FsRequestSync(port, PORT_ACCEPT, &params, sizeof(params), &op))
        return params.port_accept.client;
    else
    {
        errno = op.result;
        return NULL;
    }
}
