/* $Id: port.c,v 1.3 2002/01/15 00:13:06 pavlovskii Exp $ */

#include <errno.h>
#include <wchar.h>

#include <os/syscall.h>
#include <os/device.h>

/*bool PortListen(handle_t port)
{
	request_port_t req;
	req.header.code = PORT_LISTEN;
	req.params.port_listen.port = port;
	return FsRequestSync(port, (request_t*) &req);
}

bool PortConnect(handle_t port, const wchar_t *remote)
{
	request_port_t req;
	req.header.code = PORT_CONNECT;
	req.params.port_connect.port = port;
	req.params.port_connect.remote = remote;
	req.params.port_connect.name_size = (wcslen(remote) + 1) * sizeof(wchar_t);
	return FsRequestSync(port, (request_t*) &req);
}

handle_t PortAccept(handle_t port, uint32_t flags)
{
	request_port_t req;
	req.header.code = PORT_ACCEPT;
	req.params.port_accept.port = port;
	req.params.port_accept.flags = flags;
	if (FsRequestSync(port, (request_t*) &req))
		return req.params.port_accept.client;
	else
	{
		errno = req.header.result;
		return NULL;
	}
}
*/