/* $Id: syscall.h,v 1.6 2002/03/04 18:56:07 pavlovskii Exp $ */
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
struct sysinfo_t;
struct systimes_t;
struct context_v86_t;

#include <os/sysdef.h>

#ifdef __cplusplus
}
#endif

#endif