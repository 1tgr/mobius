/* $Id: port.c,v 1.1.1.1 2002/12/21 09:50:26 pavlovskii Exp $ */

#include <errno.h>
#include <wchar.h>

#include <os/syscall.h>
#include <os/device.h>
#include <os/defs.h>
#include <os/rtl.h>

bool PortListen(handle_t port)
{
    params_port_t params;
    params.port_listen.port = port;
    return FsRequestSync(port, PORT_LISTEN, &params, sizeof(params), NULL);
}

bool PortConnect(handle_t port, const wchar_t *remote)
{
    params_port_t params;
    params.port_connect.port = port;
    params.port_connect.remote = remote;
    params.port_connect.name_size = (wcslen(remote) + 1) * sizeof(wchar_t);
    return FsRequestSync(port, PORT_CONNECT, &params, sizeof(params), NULL);
}

handle_t PortAccept(handle_t port, uint32_t flags)
{
    params_port_t params;
    params.port_accept.port = port;
    params.port_accept.flags = flags;
    if (FsRequestSync(port, PORT_ACCEPT, &params, sizeof(params), NULL))
        return params.port_accept.client;
    else
        return NULL;
}
