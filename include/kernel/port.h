#ifndef __KERNEL_PORT_H
#define __KERNEL_PORT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/proc.h>
#include <kernel/fs.h>
#include <kernel/thread.h>

#define PORT_CONNECTED			1
#define PORT_PENDING_CONNECT	2
#define PORT_LISTENING			4

typedef struct port_t port_t;
struct port_t
{
	file_t file;
	port_t *prev, *next;
	wchar_t* name;
	process_t* owner;

	dword state;

	union
	{
		struct
		{
			port_t* remote;
			size_t buffer_length;
			byte buffer[128];
		} client;

		struct
		{
			semaphore_t connect;
			port_t *connect_first, *connect_last;
			int num_clients;
		} server;
	} u;
};

port_t* portCreate(process_t* owner, const wchar_t* name);
void portDelete(port_t* port);
bool portListen(port_t* port);
bool portConnect(port_t* port, const wchar_t* remote);
port_t* portAccept(port_t* server);

#ifdef __cplusplus
}
#endif

#endif