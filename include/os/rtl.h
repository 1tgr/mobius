/* $Id: rtl.h,v 1.6 2002/03/14 01:26:52 pavlovskii Exp $ */
#ifndef __OS_RTL_H
#define __OS_RTL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <os/defs.h>

bool	    FsFullPath(const wchar_t* src, wchar_t* dst);
wchar_t *   ProcGetCwd();
thread_info_t *
	    ThrGetThreadInfo(void);
process_info_t *
	    ProcGetProcessInfo(void);
addr_t	    ProcGetExeBase(void);
const void *ResFindResource(addr_t base, uint16_t type, uint16_t id, uint16_t language);
bool	    ResLoadString(uint32_t base, uint16_t id, wchar_t* str, size_t str_max);
size_t      ResSizeOfResource(addr_t base, uint16_t type, uint16_t id, uint16_t language);
uint32_t    ConReadKey(void);

#ifdef __cplusplus
}
#endif

#endif