/* $Id: wprintf.c,v 1.1 2002/12/21 09:50:09 pavlovskii Exp $ */

#include <stdlib.h>
#include <stddef.h>
#include <printf.h>
#include <stdio.h>
#include <wchar.h>

int _cputws(const wchar_t *str, size_t count);

/*#define BUFFER*/

#ifdef BUFFER
static wchar_t printf_buffer[80], *ch;
#endif

static bool kprintfhelp(void* pContext, const wchar_t* str, size_t len)
{
#ifdef BUFFER
    if (ch + len > printf_buffer + _countof(printf_buffer))
    {
	_cputws(printf_buffer, ch - printf_buffer);
	ch = printf_buffer;
    }

    memcpy(ch, str, sizeof(wchar_t) * len);
    ch += len;
    
    if (*str == L'\r' || *str == L'\n')
    {
	_cputws(printf_buffer, ch - printf_buffer);
	ch = printf_buffer;
    }

#else
    _cputws(str, len);
#endif
    return true;
}

/*!    \brief Prints a formatted string to the standard output, given a va_list of 
 *	  arguments.
 *    Use this function instead of wprintf() when a va_list is used, rather 
 *	  than a variable list of parameters in the argument list.
  *    \param	 fmt	The string to be processed. Conforms to normal wprintf() 
 *	  specifications.
 *    \param	ptr    A va_list of the arguments that control the output.
 *    \return	 The total number of characters output.
 */
int vwprintf(const wchar_t* fmt, va_list ptr)
{
    int ret;
#ifdef BUFFER
    if (ch == NULL)
	ch = printf_buffer;
#endif
    ret = dowprintf(kprintfhelp, NULL, fmt, ptr);
/*#ifdef BUFFER
    if (ch > printf_buffer)
	_cputws(printf_buffer, ch - printf_buffer);
#endif*/
    return ret;
}

/*!    \brief Prints a formatted string to the standard output.
 *
 *    \param	fmt    The string to be processed. Conforms to normal wprintf() 
 *	  specifications.
 *    \param	...    Arguments that control the output.
 *    \return	 The total number of characters output.
 */
int wprintf(const wchar_t* fmt, ...)
{
    va_list ptr;
    int ret;

    va_start(ptr, fmt);
    ret = vwprintf(fmt, ptr);
    va_end(ptr);
    return ret;
}