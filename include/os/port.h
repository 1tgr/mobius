/* $Id: port.h,v 1.2 2003/06/05 21:58:16 pavlovskii Exp $ */
#ifndef __OS_PORT_H
#define __OS_PORT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

bool	PortListen(handle_t port);
bool	PortConnect(handle_t port, const wchar_t *remote, uint32_t flags);
handle_t	PortAccept(handle_t port, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
