/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/debug.h>

#include <stdio.h>
#include <printf.h>
#include <string.h>

typedef struct wprintf_buf_t wprintf_buf_t;
struct wprintf_buf_t
{
	wchar_t *ch;
	wchar_t buffer[80];
};


static bool wdprintfhelp(void *cookie, const wchar_t* str, size_t len)
{
	wprintf_buf_t *buf;

	buf = cookie;
    if (buf->ch + len > buf->buffer + _countof(buf->buffer))
    {
		DbgWrite(buf->buffer, buf->ch - buf->buffer);
		buf->ch = buf->buffer;
    }

    memcpy(buf->ch, str, sizeof(wchar_t) * len);
    buf->ch += len;
    return true;
}


int vwprintf(const wchar_t* fmt, va_list ptr)
{
    int ret;
	wprintf_buf_t buf;
	buf.ch = buf.buffer;
    ret = dowprintf(wdprintfhelp, &buf, fmt, ptr);
    if (buf.ch > buf.buffer)
		DbgWrite(buf.buffer, buf.ch - buf.buffer);
    return ret;
}


int wprintf(const wchar_t* fmt, ...)
{
    va_list ptr;
    int ret;

    va_start(ptr, fmt);
    ret = vwprintf(fmt, ptr);
    va_end(ptr);
    return ret;
}
