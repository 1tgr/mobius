/* $Id: arch.h,v 1.2 2003/06/05 21:58:16 pavlovskii Exp $ */
#ifndef __KERNEL_ARCH_H
#define __KERNEL_ARCH_H

#ifdef i386
#include "i386.h"
#elif defined(WIN32)
#include "win32.h"
#else
#error Must specify architecture in makefile
#endif

#ifdef __cplusplus
extern "C"
{
#endif

struct thread_t;
struct context_t;

/*!
 *	\ingroup	kernel
 *	\defgroup	arch	System Architecture
 *	@{
 */

void    ArchSetupContext(struct thread_t *thr, addr_t entry, bool isKernel, 
                         bool useParam, addr_t param, addr_t stack);
void    ArchProcessorIdle(void);
void    ArchMaskIrq(uint16_t enable, uint16_t disable);
void    ArchDbgBreak(void);
void    ArchDbgDumpContext(const struct context_t* ctx);
bool    ArchAttachToThread(struct thread_t *thr, bool isNewAddressSpace);
void    ArchReboot(void);
void    ArchPowerOff(void);
void    ArchMicroDelay(unsigned microseconds);
unsigned ArchThisCpu(void);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif
