/* $Id: rtl.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
#ifndef __OS_RTL_H
#define __OS_RTL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/* Routines implemented in librtl */
bool		FsFullPath(const wchar_t* src, wchar_t* dst);

/* Support routines to be implemented */
wchar_t *	ProcGetCwd();

#ifdef __cplusplus
}
#endif

#endif