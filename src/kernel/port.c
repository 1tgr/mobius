#include <kernel/kernel.h>
#include <kernel/port.h>
#include <errno.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

SEMAPHORE(server_sem);
port_t *server_first, *server_last;

bool portRequest(device_t* dev, request_t* req);
device_t port_dev =
{
	NULL,
	portRequest,
	NULL, NULL,
	NULL
};

bool portRequest(device_t* dev, request_t* req)
{
	port_t *port, *remote;
	dword *params;

	assert(dev == &port_dev);

	switch (req->code)
	{
	case FS_WRITE:
		port = (port_t*) req->params.fs_write.fd;
		if ((port->state & PORT_CONNECTED) == 0 ||
			port->u.client.remote == NULL)
		{
			wprintf(L"%s: not connected\n", port->name);
			req->code = EINVALID;
			return false;
		}

		//wprintf(L"port: write to %s->%s\n", 
			//port->name, port->u.client.remote->name);
		remote = port->u.client.remote;

		memcpy(remote->u.client.buffer + remote->u.client.buffer_length, 
			(void*) req->params.fs_write.buffer,
			req->params.fs_write.length);
		remote->u.client.buffer_length += req->params.fs_write.length;
		hndSignal(remote, remote->u.client.buffer_length > 0);

		hndSignal(req->event, true);
		return true;

	case FS_READ:
		port = (port_t*) req->params.fs_read.fd;
		if ((port->state & PORT_CONNECTED) == 0 ||
			port->u.client.remote == NULL)
		{
			wprintf(L"%s: not connected\n", port->name);
			req->code = EINVALID;
			return false;
		}

		req->params.fs_read.length = 
			min(req->params.fs_read.length, port->u.client.buffer_length);
		memcpy((void*) req->params.fs_read.buffer, 
			port->u.client.buffer,
			req->params.fs_read.length);

		port->u.client.buffer_length -= req->params.fs_read.length;
		memmove(port->u.client.buffer, 
			port->u.client.buffer + req->params.fs_read.length,
			port->u.client.buffer_length);
		hndSignal(port, port->u.client.buffer_length > 0);

		//wprintf(L"port: read from %s->%s\n", 
			//port->name, port->u.client.remote->name);
		
		hndSignal(req->event, true);
		return true;

	case FS_IOCTL:
		port = (port_t*) req->params.fs_ioctl.fd;
		params = req->params.fs_ioctl.buffer;
		switch (req->params.fs_ioctl.code)
		{
		case 0:
			params[0] = port->u.client.buffer_length;
			req->params.fs_ioctl.length = sizeof(dword);
			hndSignal(req->event, true);
			return true;
		default:
			req->params.fs_ioctl.length = 0;
			break;
		}
	}

	req->code = ENOTIMPL;
	return false;
}

port_t* portFindServer(const wchar_t* name)
{
	port_t *port;

	for (port = server_first; port; port = port->next)
		if (wcsicmp(port->name, name) == 0)
			return port;
	
	return NULL;
}

port_t* portCreate(process_t* owner, const wchar_t* name)
{
	port_t* port;

	port = hndAlloc(sizeof(port_t), owner);
	assert(port != NULL);

	port->file.fsd = &port_dev;
	port->file.pos = 0;
	port->owner = owner;
	port->state = 0;

	if (name)
		port->name = wcsdup(name);
	else
		port->name = wcsdup(L"");

	//wprintf(L"portCreate: created port %p = %s\n", port, name);
	return port;
}

void portDelete(port_t* port)
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
	hndFree(port);
}

bool portListen(port_t* port)
{
	if (port->state != 0)
		return false;

	port->state |= PORT_LISTENING;
	port->u.server.connect_first = port->u.server.connect_last = NULL;
	port->u.server.num_clients = 0;
	semInit(&port->u.server.connect);
	//wprintf(L"portListen: %p = %s\n", port, port->name);

	semAcquire(&server_sem);
	if (server_last)
		server_last->next = port;
	if (server_first == NULL)
		server_first = port;
	port->next = NULL;
	port->prev = server_last;
	server_last = port;
	semRelease(&server_sem);
	return true;
}

bool portConnect(port_t* port, const wchar_t* remote)
{
	port_t *remote_port;
	
	if (port->state != 0)
		return false;

	remote_port = portFindServer(remote);
	if (remote_port == NULL)
	{
		//wprintf(L"%s: remote port not found\n", remote);
		errno = ENOTFOUND;
		return false;
	}

	if ((remote_port->state & PORT_LISTENING) == 0)
	{
		wprintf(L"%s: remote port not listening\n", remote);
		errno = EINVALID;
		return false;
	}
	
	//wprintf(L"portConnect: %s to %s\n", port->name, remote_port->name);
	port->u.client.remote = remote_port;
	port->u.client.buffer_length = 0;

	//wprintf(L"Acquiring sem...");
	semAcquire(&port->u.server.connect);
	//wprintf(L"done\n");

	if (remote_port->u.server.connect_last)
		remote_port->u.server.connect_last->next = port;
	remote_port->u.server.connect_last = port;
	if (remote_port->u.server.connect_first == NULL)
		remote_port->u.server.connect_first = port;

	port->next = NULL;
	port->prev = remote_port->u.server.connect_last;

	port->state = PORT_PENDING_CONNECT;
	hndSignal(remote_port, true);
	hndSignal(port, false);
	semRelease(&port->u.server.connect);
	//wprintf(L"portConnect: finished\n");
	return true;
}

port_t* portAccept(port_t* server)
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
	//wprintf(L"portAccept: %s, %s\n", server->name, client->name);

	/* Acquire the server's connection semaphore */
	//wprintf(L"Acquiring sem...");
	semAcquire(&server->u.server.connect);
	//wprintf(L"done\n");
	
	/* Unlink the client from the server's queue */
	if (client->next)
		client->next->prev = NULL;
	client->next = client->prev = NULL;
	server->u.server.connect_first = client->next;

	/* Create a new thread to connect with the client */
	swprintf(name, L"%s_client%d", server->name, ++server->u.server.num_clients);
	port = portCreate(server->owner, name);

	/*
	 * Connect the new port with the client --
	 *	we must do manually what portConnect does
	 */
	port->state = PORT_CONNECTED;
	port->u.client.remote = client;
	port->u.client.buffer_length = 0;

	/* Connect the client with the new port */
	client->state = PORT_CONNECTED;
	client->u.client.remote = port;

	/* Re-signal the server if it has another client waiting */
	hndSignal(server, server->u.server.connect_first != NULL);

	/* Signal the client because it is now connected */
	hndSignal(client, true);
	semRelease(&server->u.server.connect);
	//wprintf(L"portAccept: finished\n");

	/* Return the port connected to the client */
	return port;
}
