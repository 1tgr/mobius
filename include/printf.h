/* $Id: printf.h,v 1.1.1.1 2002/12/31 01:26:20 pavlovskii Exp $ */
#ifndef __PRINTF_H
#define __PRINTF_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <stdarg.h>

/*!
 *  \ingroup	libc
 *  \defgroup	printf	printf() implementation
 *  @{
 */

typedef bool (*PRINTFUNC) (void* pContext, const char* str, size_t dwLen);
typedef bool (*WPRINTFUNC) (void* pContext, const wchar_t* str, size_t dwLen);

int doprintf(PRINTFUNC func, void* pContext, const char* fmt, va_list ptr);
int dowprintf(WPRINTFUNC func, void* pContext, const wchar_t* fmt, va_list ptr);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif
