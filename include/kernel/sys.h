#ifndef __KERNEL_SYS_H
#define __KERNEL_SYS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/driver.h>

struct context_t;

void*	sysOpen(const wchar_t* filespec);
void	sysMount(const wchar_t* path, void* ptr);
dword	sysInvoke(void* ptr, dword method, dword* params, size_t sizeof_params);
dword	sysUpTime();
void	sysYield();
bool	sysV86Fault(struct context_t* ctx);
void	sysInit();

#define SHUTDOWN_POWEROFF	0
#define SHUTDOWN_REBOOT		1
bool	keShutdown(dword flags);

#ifdef __cplusplus
}
#endif

#endif