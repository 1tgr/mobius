/* $Id: io.h,v 1.2 2002/02/20 01:35:52 pavlovskii Exp $ */
#ifndef __KERNEL_IO_H
#define __KERNEL_IO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <os/device.h>

#ifndef __DEVICE_T_DEFINED
typedef struct device_t device_t;
#define __DEVICE_T_DEFINED
#endif

/*!
 *	\ingroup	kernel
 *	\defgroup	io	Device I/O
 *	@{
 */

/* Device interface routines */
bool	IoRequest(device_t *from, device_t *dev, request_t *req);
bool	IoRequestSync(device_t *dev, request_t *req);
struct device_t *	IoOpenDevice(const wchar_t *name);
void	IoCloseDevice(device_t *dev);
size_t	IoReadSync(device_t *dev, uint64_t ofs, void *buf, size_t size);
void	IoNotifyCompletion(request_t *req);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif