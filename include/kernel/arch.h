/* $Id: arch.h,v 1.6 2002/04/04 00:08:42 pavlovskii Exp $ */
#ifndef __KERNEL_ARCH_H
#define __KERNEL_ARCH_H

#ifdef i386
#include "i386.h"
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

void	ArchSetupContext(struct thread_t *thr, addr_t entry, bool isKernel, 
						 bool useParam, addr_t param, addr_t stack);
void	ArchProcessorIdle(void);
void	ArchMaskIrq(uint16_t enable, uint16_t disable);
void	ArchDbgBreak(void);
void	ArchDbgDumpContext(const struct context_t* ctx);
bool	ArchAttachToThread(struct thread_t *thr, bool isNewAddressSpace);
void    ArchReboot(void);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif