/* $Id: defs.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
#ifndef __OS_DEFS_H
#define __OS_DEFS_H

#include <sys/types.h>

#define SYS_BOOT	L"/System/Boot"
#define SYS_PORTS	L"/System/Ports"
#define SYS_DEVICES	L"/System/Devices"

#define MAX_PATH	256

#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif

#ifndef __KERNEL_MEMORY_H
#define PAGE_ALIGN(addr)	((addr) & -PAGE_SIZE)
#define PAGE_ALIGN_UP(addr)	(((addr) + PAGE_SIZE - 1) & -PAGE_SIZE)
#endif

/* Flags for FsCreate() and FsOpen() */
#define FILE_READ	1
#define FILE_WRITE	2

typedef struct process_info_t process_info_t;
struct process_info_t
{
	wchar_t cwd[256];
	unsigned id;
	addr_t base;
	handle_t std_in, std_out;
};

typedef struct thread_info_t thread_info_t;
struct thread_info_t
{
	thread_info_t *info;
	uint32_t id;
	process_info_t *process;
};

#endif