/* $Id: cputs.c,v 1.5 2002/02/27 18:33:55 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/rtl.h>

#include <wchar.h>
#include <errno.h>

process_info_t *ProcGetProcessInfo(void);

handle_t __cputws_event;

int _cputs(const char *str, size_t count)
{
	fileop_t op;
	bool ret;
	
	if (__cputws_event == NULL)
		__cputws_event = op.event = EvtAlloc();
	else
		op.event = __cputws_event;

	ret = FsWrite(ProcGetProcessInfo()->std_out,
		str,
		count * sizeof(wchar_t), 
		&op);

	if (!ret)
		return 0;
	else
	{
		if (op.result == SIOPENDING)
			ThrWaitHandle(__cputws_event);
		return op.bytes / sizeof(wchar_t);
	}
}