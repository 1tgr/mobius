/* $Id: arch.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
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

bool	ArchInit(void);
void	ArchSetupContext(struct thread_t *thr, addr_t entry, bool isKernel, 
						 bool useParam, addr_t param, addr_t stack);
void	ArchProcessorIdle(void);
void	ArchMaskIrq(uint16_t enable, uint16_t disable);
void	ArchDbgBreak(void);
void	ArchDbgDumpContext(const struct context_t* ctx);
struct thread_t *	ArchAttachToThread(struct thread_t *thr);

/* this is in device.c but is only called on a context switch */
void	DevRunHandlers(void);

#ifdef __cplusplus
}
#endif

#endif