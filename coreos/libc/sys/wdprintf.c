/* $Id: wdprintf.c,v 1.1.1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <os/syscall.h>
#include <wchar.h>
#include <stdarg.h>
#include <stdio.h>

int _vwdprintf(const wchar_t *str, va_list argptr)
{
    static wchar_t buf[1024];
    int chars;
    chars = vswprintf(buf, str, argptr);
    DbgWrite(buf, chars);
    return chars;
}

int _wdprintf(const wchar_t *fmt, ...)
{
    va_list ptr;
    int ret;

    va_start(ptr, fmt);
    ret = _vwdprintf(fmt, ptr);
    va_end(ptr);
    return ret;
}
