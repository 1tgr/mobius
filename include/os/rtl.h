/* $Id: rtl.h,v 1.3 2002/02/20 01:35:52 pavlovskii Exp $ */
#ifndef __OS_RTL_H
#define __OS_RTL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <os/defs.h>

/* Routines implemented in librtl */
bool			FsFullPath(const wchar_t* src, wchar_t* dst);

/* Support routines to be implemented */
wchar_t *		ProcGetCwd();
thread_info_t	*ThrGetThreadInfo(void);
process_info_t*	ProcGetProcessInfo(void);

#ifdef __cplusplus
}
#endif

#endif