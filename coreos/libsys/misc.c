/* $Id: misc.c,v 1.1.1.1 2002/12/21 09:50:26 pavlovskii Exp $ */

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
