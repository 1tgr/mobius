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