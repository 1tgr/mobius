#ifndef __KERNEL_INIT_H
#define __KERNEL_INIT_H

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 *	\ingroup	kernel
 *	\defgroup	init	Initialization Routines
 *
 *	The functions in this file are each called from \p KernelMain on system
 *	startup. They are kept in a separate header file because \p KernelMain
 *	is the only thing that calls these routines.
 *
 *	@{
 */

bool	__initcode ArchInit(void);
bool	__initcode FsInit(void);
bool	__initcode MemInit(void);
bool	__initcode ProcInit(void);
bool	__initcode RdInit(void);
bool    __initcode ThrInit(void);
bool    __initcode RtlInit(void);

#include <kernel/fs.h>
fsd_t *  RdMountFs(driver_t* driver, const wchar_t *dest);
fsd_t *  DevMountFs(driver_t *driver, const wchar_t *dest);
fsd_t *  PortMountFs(driver_t *driver, const wchar_t *dest);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif