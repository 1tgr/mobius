/* $Id: cache.h,v 1.1 2002/01/05 00:56:15 pavlovskii Exp $ */
#ifndef __KERNEL_CACHE_H
#define __KERNEL_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

struct device_t;

struct device_t *CcInstallBlockCache(struct device_t *dev, uint32_t block_size);

#ifdef __cplusplus
}
#endif

#endif