#include <os/defs.h>

thread_info_t *ThrGetThreadInfo(void)
{
	thread_info_t *info;
	__asm__("mov %%fs:(0), %0" : "=g" (info));
	return info;
}

process_info_t *ProcGetProcessInfo(void)
{
	return ThrGetThreadInfo()->process;
}