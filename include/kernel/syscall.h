/* $Id$ */
#ifndef __KERNEL_SYSCALL_H
#define __KENREL_SYSCALL_H

/*
 * This file is identical to os/syscall.h, but defines the prototypes for the 
 *	kernel-mode syscall wrappers.
 * Do not use os/syscall.h in kernel mode, since some kernel functions with 
 *  the same names have different prototypes.
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

#define SYSCALL(rtn, name, argbytes, args)	rtn Wrap##name args;

struct fileop_t;
struct process_info_t;
struct sysinfo_t;
struct systimes_t;
struct context_v86_t;
struct dirent_t;

#include <os/sysdef.h>

#ifdef __cplusplus
}
#endif

#endif
