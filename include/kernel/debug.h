/* $Id: debug.h,v 1.6 2002/08/17 23:09:01 pavlovskii Exp $ */
#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/*!
 *	\ingroup	kernel
 *	\defgroup	dbg	Debugging
 *	@{
 */

#ifdef DEBUG
#include <stdio.h>
#define TRACE0(f)		wprintf(L##f)
#define TRACE1(f, a)	wprintf(L##f, a)
#define TRACE2(f, a, b)	wprintf(L##f, a, b)
#define TRACE3(f, a, b, c)	wprintf(L##f, a, b, c)
#define TRACE4(f, a, b, c, d)	wprintf(L##f, a, b, c, d)
#define TRACE5(f, a, b, c, d, e)	wprintf(L##f, a, b, c, d, e)
#else
#define TRACE0(f)
#define TRACE1(f, a)
#define TRACE2(f, a, b)
#define TRACE3(f, a, b, c)
#define TRACE4(f, a, b, c, d)
#define TRACE5(f, a, b, c, d, e)
#endif

/*typedef bool (CDECL *DBGATTACH)(thread_t*, context_t*, addr_t);*/

#include <os/coff.h>

struct module_t;
struct process_t;
struct context_t;

bool	DbgLookupSymbol(struct module_t *mod, void* sym, addr_t* address, 
						SYMENT *syment);
char *	DbgGetStringsTable(struct module_t *mod);
char *	DbgGetSymbolName(void *strings, SYMENT *se);
struct module_t *	DbgLookupModule(struct process_t *proc, addr_t addr);
bool	DbgIsValidEsp(struct process_t *proc, uint32_t _esp);
const wchar_t *	DbgFormatAddress(uint32_t flags, uint32_t seg, uint32_t ofs);

void	DbgDumpStack(struct process_t *proc, uint32_t _ebp);
void	DbgDumpModules(struct process_t *proc);
void	DbgDumpThreads(void);
void	DbgDumpContext(const struct context_t* ctx);
void	DbgLookupLineNumber(addr_t base, void *rawdata, void *symbol, 
							unsigned *line, char** file);
void	DbgDumpVmm(struct process_t *proc);
void	DbgDumpBuffer(const void* buf, size_t size);
bool    DbgStartShell(void);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif