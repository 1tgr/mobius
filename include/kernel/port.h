/* $Id: port.h,v 1.4 2002/02/24 19:13:11 pavlovskii Exp $ */
#ifndef __KERNEL_PORT_H
#define __KERNEL_PORT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/fs.h>
#include <kernel/thread.h>

/*!
 *	\ingroup	kernel
 *	\defgroup	port	Ports
 *	@{
 */

#define PORT_CONNECTED			1
#define PORT_PENDING_CONNECT	2
#define PORT_LISTENING			4

typedef struct port_t port_t;
struct port_t
{
	handle_hdr_t hdr;

	file_t file;
	bool is_search;
	port_t *prev, *next;
	wchar_t* name;
	process_t* owner;

	unsigned state;

	union
	{
		struct
		{
			port_t *remote;
			size_t buffer_length;
			uint8_t buffer[128];
		} client;

		struct
		{
			semaphore_t connect;
			port_t *connect_first, *connect_last;
			int num_clients;
		} server;
	} u;
};

CASSERT(offsetof(port_t, file) == sizeof(handle_hdr_t));

/*! @} */

#ifdef __cplusplus
}
#endif

#endif