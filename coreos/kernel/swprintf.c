/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/debug.h>

#include <stdio.h>
#include <printf.h>
#include <string.h>


static bool swprintfhelp(void *cookie, const wchar_t* str, size_t len)
{
	wchar_t **ch;

	ch = cookie;
    memcpy(*ch, str, sizeof(wchar_t) * len);
    *ch += len;
    return true;
}


int vswprintf(wchar_t *buf, const wchar_t* fmt, va_list ptr)
{
    int ret;
    ret = dowprintf(swprintfhelp, &buf, fmt, ptr);
    *buf = '\0';
    return ret;
}


int swprintf(wchar_t *buf, const wchar_t* fmt, ...)
{
    va_list ptr;
    int ret;

    va_start(ptr, fmt);
    ret = vswprintf(buf, fmt, ptr);
    va_end(ptr);
    return ret;
}
