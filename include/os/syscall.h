/* $Id: syscall.h,v 1.3 2002/02/20 01:35:52 pavlovskii Exp $ */
#ifndef __OS_SYSCALL_H
#define __OS_SYSCALL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

#define SYSCALL(rtn, name, argbytes, args...)	rtn name(##args);

struct fileop_t;

#include <os/sysdef.h>

#ifdef __cplusplus
}
#endif

#endif