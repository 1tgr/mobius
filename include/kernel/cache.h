#ifndef __KERNEL_CACHE_H
#define __KERNEL_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/driver.h>

device_t* ccInstallBlockCache(device_t* dev, dword block_size);

#ifdef __cplusplus
}
#endif

#endif