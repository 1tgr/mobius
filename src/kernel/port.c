#include <kernel/kernel.h>
#include <kernel/port.h>
#include <kernel/sched.h>

#include <errno.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

semaphore_t server_sem;
port_t *server_first, *server_last;

device_t *PortMountFs(driver_t *driver, const wchar_t *name, device_t *dev);
static bool PortFsRequest(device_t *dev, request_t *req);

driver_t port_driver =
{
	NULL,
	NULL,
	NULL,
	NULL,
	PortMountFs
};

static device_t port_dev =
{
	PortFsRequest,	/* request */
	NULL,			/* isr */
	NULL,			/* finishio */
	&port_driver, 
};

device_t *PortMountFs(driver_t *driver, const wchar_t *name, device_t *dev)
{
	assert(driver == &port_driver);
	return &port_dev;
}

static port_t* PortFindServer(const wchar_t* name)
{
	port_t *port;

	for (port = server_first; port; port = port->next)
		if (_wcsicmp(port->name, name) == 0)
			return port;
	
	return NULL;
}

static port_t* PortCreate(process_t* owner, const wchar_t* name, uint32_t flags)
{
	port_t* port;

	port = malloc(sizeof(port_t));
	assert(port != NULL);

	memset(port, 0, sizeof(port_t));
	HndInit(&port->hdr, 'file');
	port->file.fsd = &port_dev;
	port->file.pos = 0;
	port->file.flags = flags;
	port->owner = owner;
	port->state = 0;

	if (name)
		port->name = _wcsdup(name);
	else
		port->name = _wcsdup(L"");

	/*wprintf(L"PortCreate: created port %p = %s\n", port, name);*/
	return port;
}

static void PortDelete(port_t* port)
{
	port_t *server, *next;

	for (server = server_first; server; server = next)
	{
		next = server->next;

		if (server == port)
		{
			if (server->next)
				server->next->prev = server->prev;
			if (server->prev)
				server->prev->next = server->next;
			if (server_first == server)
				server_first = server->next;
			if (server_last == server)
				server_last = server->prev;
			break;
		}
	}

	free(port->name);
	free(port);
}

static bool PortListen(port_t* port)
{
	if (port->state != 0)
		return false;

	port->state |= PORT_LISTENING;
	port->u.server.connect_first = port->u.server.connect_last = NULL;
	port->u.server.num_clients = 0;
	SemInit(&port->u.server.connect);
	/*wprintf(L"PortListen: %p = %s\n", port, port->name);*/

	SemAcquire(&server_sem);
	if (server_last)
		server_last->next = port;
	if (server_first == NULL)
		server_first = port;
	port->next = NULL;
	port->prev = server_last;
	server_last = port;
	SemRelease(&server_sem);
	return true;
}

static bool PortConnect(port_t* port, const wchar_t* remote)
{
	port_t *remote_port;
	
	if (port->state != 0)
		return false;

	remote_port = PortFindServer(remote);
	if (remote_port == NULL)
	{
		wprintf(L"%s: remote port not found\n", remote);
		errno = ENOTFOUND;
		return false;
	}

	if ((remote_port->state & PORT_LISTENING) == 0)
	{
		wprintf(L"%s: remote port not listening\n", remote);
		errno = EINVALID;
		return false;
	}
	
	/*wprintf(L"PortConnect: %s to %s\n", port->name, remote_port->name);*/
	port->u.client.remote = remote_port;
	port->u.client.buffer_length = 0;

	/*wprintf(L"Acquiring sem...");*/
	MtxAcquire(&port->u.server.connect);
	/*wprintf(L"done\n");*/

	if (remote_port->u.server.connect_last)
		remote_port->u.server.connect_last->next = port;
	remote_port->u.server.connect_last = port;
	if (remote_port->u.server.connect_first == NULL)
		remote_port->u.server.connect_first = port;

	port->next = NULL;
	port->prev = remote_port->u.server.connect_last;

	port->state = PORT_PENDING_CONNECT;
	HndSignalPtr(&remote_port->hdr, true);
	HndSignalPtr(&port->hdr, false);
	MtxRelease(&port->u.server.connect);
	/*wprintf(L"PortConnect: finished\n");*/
	return true;
}

static port_t* PortAccept(port_t* server, uint32_t flags)
{
	port_t *client, *port;
	wchar_t name[50];

	/* Server port must be listening and it must have a client waiting */
	if ((server->state & PORT_LISTENING) == 0 ||
		server->u.server.connect_first == NULL)
	{
		errno = EINVALID;
		return NULL;
	}

	/* Get the first waiting client from the queue */
	client = server->u.server.connect_first;
	/*wprintf(L"PortAccept: %s, %s\n", server->name, client->name);*/

	/* Acquire the server's connection semaphore */
	/*wprintf(L"Acquiring sem..."); */
	MtxAcquire(&server->u.server.connect);
	/*wprintf(L"done\n"); */
	
	/* Unlink the client from the server's queue */
	if (client->next)
		client->next->prev = NULL;
	client->next = client->prev = NULL;
	server->u.server.connect_first = client->next;

	/* Create a new thread to connect with the client */
	swprintf(name, L"%s_client%d", server->name, ++server->u.server.num_clients);
	port = PortCreate(server->owner, name, flags);

	/*
	 * Connect the new port with the client --
	 *	we must do manually what PortConnect does
	 */
	port->state = PORT_CONNECTED;
	port->u.client.remote = client;
	port->u.client.buffer_length = 0;

	/* Connect the client with the new port */
	client->state = PORT_CONNECTED;
	client->u.client.remote = port;

	/* Re-signal the server if it has another client waiting */
	/*HndSignalPtr(&server->hdr, server->u.server.connect_first != NULL);*/

	/* Signal the client because it is now connected */
	HndSignalPtr(&client->hdr, true);
	MtxRelease(&server->u.server.connect);
	/*wprintf(L"PortAccept: finished\n");*/

	/* Return the port connected to the client */
	return port;
}

bool PortFsRequest(device_t* dev, request_t* req)
{
	request_fs_t *req_fs = (request_fs_t*) req;
	request_port_t *req_port = (request_port_t*) req;
	port_t *port, *remote;
	uint32_t *params;
	
	assert(dev == &port_dev);

	switch (req->code)
	{
	case FS_CREATE:
		if (req_fs->params.fs_create.name[0] == '/')
			req_fs->params.fs_create.name++;

		port = PortCreate(current->process, req_fs->params.fs_create.name,
			req_fs->params.fs_create.flags);
		if (port)
		{
			req_fs->params.fs_create.file = 
				HndDuplicate(NULL, &port->hdr);
			/*wprintf(L"PortFsRequest(FS_CREATE): port = %u\n", 
				req_fs->params.fs_create.file);*/
			return true;
		}
		else
		{
			req->result = errno;
			return false;
		}

	case FS_OPEN:
		if (req_fs->params.fs_open.name[0] == '/')
			req_fs->params.fs_open.name++;

		port = PortCreate(current->process, NULL, req_fs->params.fs_open.flags);
		if (port == NULL)
		{
			req->result = errno;
			return false;
		}

		if (!PortConnect(port, req_fs->params.fs_open.name))
		{
			PortDelete(port);
			req->result = errno;
			return false;
		}

		req_fs->params.fs_open.file = HndDuplicate(NULL, &port->hdr);
		/*wprintf(L"PortFsRequest(FS_OPEN): port = %u\n", 
				req_fs->params.fs_open.file);*/

		ThrWaitHandle(current, req_fs->params.fs_open.file, 0);
		ScNeedSchedule(true);
		return true;

	case FS_CLOSE:
		port = (port_t*) 
			HndGetPtr(NULL, req_fs->params.fs_close.file, 'file');
		if (port == NULL)
		{
			req->code = EINVALID;
			return false;
		}

		HndRemovePtrEntries(NULL, &port->hdr);
		HndFreePtr(&port->hdr);
		PortDelete(port);
		return true;

	case FS_WRITE:
		port = (port_t*) 
			HndGetPtr(NULL, req_fs->params.fs_write.file, 'file');
		if (port == NULL)
		{
			req->code = EINVALID;
			return false;
		}

		if ((port->state & PORT_CONNECTED) == 0 ||
			port->u.client.remote == NULL)
		{
			wprintf(L"%s: not connected\n", port->name);
			req->code = errno;
			return false;
		}

		/*wprintf(L"port: write to %s->%s\n", 
			port->name, port->u.client.remote->name);*/
		remote = port->u.client.remote;

		memcpy(remote->u.client.buffer + remote->u.client.buffer_length, 
			(void*) req_fs->params.fs_write.buffer,
			req_fs->params.fs_write.length);
		remote->u.client.buffer_length += req_fs->params.fs_write.length;
		/*HndSignalPtr(&remote->hdr, remote->u.client.buffer_length > 0);*/
		if (remote->hdr.signals == 0)
			HndSignalPtr(&remote->hdr, true);
		return true;

	case FS_READ:
		port = (port_t*) 
			HndGetPtr(NULL, req_fs->params.fs_read.file, 'file');
		if (port == NULL)
		{
			req->code = EINVALID;
			return false;
		}

		if ((port->state & PORT_CONNECTED) == 0 ||
			port->u.client.remote == NULL)
		{
			wprintf(L"%s: not connected\n", port->name);
			req->code = EINVALID;
			return false;
		}

		req_fs->params.fs_read.length = 
			min(req_fs->params.fs_read.length, port->u.client.buffer_length);
		memcpy((void*) req_fs->params.fs_read.buffer, 
			port->u.client.buffer,
			req_fs->params.fs_read.length);

		port->u.client.buffer_length -= req_fs->params.fs_read.length;
		memmove(port->u.client.buffer, 
			port->u.client.buffer + req_fs->params.fs_read.length,
			port->u.client.buffer_length);
		/*HndSignalPtr(&port->hdr, port->u.client.buffer_length > 0);*/
		if (port->hdr.signals > 0 &&
			port->u.client.buffer_length)
			HndSignalPtr(&port->hdr, false);

		/*wprintf(L"port: read from %s->%s\n",
			port->name, port->u.client.remote->name);*/
		return true;

	case FS_IOCTL:
		port = (port_t*)
			HndGetPtr(NULL, req_fs->params.fs_ioctl.file, 'file');
		if (port == NULL)
		{
			req->result = EINVALID;
			return false;
		}

		params = req_fs->params.fs_ioctl.buffer;
		switch (req_fs->params.fs_ioctl.code)
		{
		case 0:
			params[0] = port->u.client.buffer_length;
			req_fs->params.fs_ioctl.length = sizeof(uint32_t);
			return true;
		default:
			req_fs->params.fs_ioctl.length = 0;
			break;
		}

	case PORT_CONNECT:
		port = (port_t*)
			HndGetPtr(NULL, req_port->params.port_connect.port, 'file');
		if (port == NULL)
		{
			req->result = EINVALID;
			return false;
		}

		if (!PortConnect(port, req_port->params.port_connect.remote))
		{
			req->result = errno;
			return false;
		}

		return true;

	case PORT_LISTEN:
		port = (port_t*)
			HndGetPtr(NULL, req_port->params.port_listen.port, 'file');
		if (port == NULL)
		{
			req->result = EINVALID;
			return false;
		}

		if (!PortListen(port))
		{
			req->result = errno;
			return false;
		}

		return true;

	case PORT_ACCEPT:
		port = (port_t*)
			HndGetPtr(NULL, req_port->params.port_accept.port, 'file');
		if (port == NULL)
		{
			req->result = EINVALID;
			return false;
		}

		remote = PortAccept(port, req_port->params.port_accept.flags);
		if (remote)
			req_port->params.port_accept.client = 
				HndDuplicate(NULL, &remote->hdr);
		else
		{
			req_port->params.port_accept.client = NULL;
			req->result = errno;
			return false;
		}

		return true;
	}

	req->code = ENOTIMPL;
	return false;
}

