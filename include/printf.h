#ifndef __PRINTF_H
#define __PRINTF_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include "stdarg.h"

typedef bool (*PRINTFUNC) (void* pContext, const char* str, dword dwLen);
typedef bool (*WPRINTFUNC) (void* pContext, const wchar_t* str, dword dwLen);

int doprintf(PRINTFUNC func, void* pContext, const char* fmt, va_list ptr);
int dowprintf(WPRINTFUNC func, void* pContext, const wchar_t* fmt, va_list ptr);

#ifdef __cplusplus
}
#endif

#endif