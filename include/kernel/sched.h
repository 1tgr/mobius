#ifndef __KERNEL_SCHED_H
#define __KERNEL_SCHED_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

extern unsigned sc_uptime;

void	ScSchedule(void);
void	ScEnableSwitch(bool enable);
void	ScNeedSchedule(bool need);

#ifdef __cplusplus
}
#endif

#endif