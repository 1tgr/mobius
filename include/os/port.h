#ifndef __OS_PORT_H
#define __OS_PORT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

addr_t portCreate(const wchar_t* name);
bool portListen(addr_t port);
bool portConnect(addr_t port, const wchar_t* remote);
addr_t portAccept(addr_t server);
bool portClose(addr_t port);

#ifdef __cplusplus
}
#endif

#endif