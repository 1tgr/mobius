/* $Id: syscall.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
#ifndef __OS_SYSCALL_H
#define __OS_SYSCALL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

#define SYSCALL(rtn, name, argbytes, args...)	rtn name(##args);

struct request_t;

#include <os/sysdef.h>

#ifdef __cplusplus
}
#endif

#endif