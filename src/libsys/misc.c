/* $Id: misc.c,v 1.5 2002/02/22 16:51:35 pavlovskii Exp $ */

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

wchar_t *ProcGetCwd(void)
{
	return ThrGetThreadInfo()->process->cwd;
}

addr_t ProcGetExeBase(void)
{
	return ThrGetThreadInfo()->process->base;
}