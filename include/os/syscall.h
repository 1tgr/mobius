/* $Id: syscall.h,v 1.4 2002/02/25 01:28:12 pavlovskii Exp $ */
#ifndef __OS_SYSCALL_H
#define __OS_SYSCALL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

#define SYSCALL(rtn, name, argbytes, args...)	rtn name(##args);

struct fileop_t;
struct process_info_t;

#include <os/sysdef.h>

#ifdef __cplusplus
}
#endif

#endif