/* $Id: rtl.h,v 1.11 2002/12/18 23:54:45 pavlovskii Exp $ */
#ifndef __OS_RTL_H
#define __OS_RTL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <os/defs.h>
#include <os/pe.h>

bool	    FsFullPath(const wchar_t* src, wchar_t* dst);
wchar_t *   ProcGetCwd();
thread_info_t *ThrGetThreadInfo(void);
process_info_t *ProcGetProcessInfo(void);
addr_t	    ProcGetExeBase(void);
const void *ResFindResource(addr_t base, uint16_t type, uint16_t id, uint16_t language);
bool	    ResLoadString(addr_t base, uint16_t id, wchar_t* str, size_t str_max);
size_t      ResSizeOfResource(addr_t base, uint16_t type, uint16_t id, uint16_t language);
size_t      ResGetStringLength(addr_t base, uint16_t id);
bool        FsReadSync(handle_t file, void *buf, size_t bytes, size_t *bytes_read);
bool        FsWriteSync(handle_t file, const void *buf, size_t bytes, size_t *bytes_written);
bool        FsRequestSync(handle_t file, uint32_t code, void *buf, size_t bytes, size_t *bytes_out);
uint32_t    ConReadKey(void);
module_info_t *DbgFindModule(addr_t addr);
IMAGE_PE_HEADERS *DbgGetPeHeaders(addr_t base);
IMAGE_SECTION_HEADER *DbgFindSectionByName(addr_t base, const char *name);
bool        DbgLookupLineNumber(addr_t addr, char **path, char **file, unsigned *line);

#ifdef __cplusplus
}
#endif

#endif
