/* $Id: sched.h,v 1.3 2002/02/20 01:35:52 pavlovskii Exp $ */
#ifndef __KERNEL_SCHED_H
#define __KERNEL_SCHED_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

extern unsigned sc_uptime;

/*!
 *	\ingroup	kernel
 *	\defgroup	sc	Scheduler
 *	@{
 */

void	ScSchedule(void);
void	ScEnableSwitch(bool enable);
void	ScNeedSchedule(bool need);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif