/* $Id: io.h,v 1.4 2002/08/04 17:22:39 pavlovskii Exp $ */
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

struct fsd_t;
struct request_t;

typedef struct io_callback_t io_callback_t;
struct io_callback_t
{
    unsigned type;
    union
    {
        device_t *dev;
        struct fsd_t *fsd;
        void (*function)(struct request_t *req);
    } u;
};

#define IO_CALLBACK_NONE        0
#define IO_CALLBACK_DEVICE      1
#define IO_CALLBACK_FSD         2
#define IO_CALLBACK_FUNCTION    3

struct page_array_t;

bool    IoRequest(const io_callback_t *cb, device_t *dev, struct request_t *req);
bool    IoRequestSync(device_t *dev, struct request_t *req);
struct device_t *IoOpenDevice(const wchar_t *name);
void    IoCloseDevice(device_t *dev);
size_t  IoReadPhysicalSync(device_t *dev, uint64_t ofs, struct page_array_t *buf, size_t size);
size_t  IoReadSync(device_t *dev, uint64_t ofs, void *buf, size_t size);
void    IoNotifyCompletion(struct request_t *req);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif