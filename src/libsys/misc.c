/* $Id: misc.c,v 1.3 2002/01/06 18:36:16 pavlovskii Exp $ */

#include <os/defs.h>

thread_info_t *ThrGetThreadInfo(void)
{
	thread_info_t *info;
	__asm__("mov %%fs:(0), %0" : "=a" (info));
	return info;
}

process_info_t *ProcGetProcessInfo(void)
{
	return ThrGetThreadInfo()->process;
}