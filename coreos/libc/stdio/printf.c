/* $Id: printf.c,v 1.1.1.1 2002/12/21 09:50:08 pavlovskii Exp $ */

#include <stddef.h>
#include <printf.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

int _cputs(const char *, size_t);

#ifdef BUFFER
static char printf_buffer[80], *ch;
#endif

static bool kprintfhelp(void* pContext, const char* str, size_t len)
{
#ifdef BUFFER
	if (ch + len > printf_buffer + _countof(printf_buffer) ||
		*str == L'\r' || *str == L'\n')
	{
		_cputs(printf_buffer);
		ch = printf_buffer;
	}

	strcpy(ch, str);
	ch += len;
#else
	_cputs(str, len);
#endif
	return true;
}

/*!	\brief Prints a formatted string to the standard output, given a va_list 
 *	of arguments.
 *
 *	Use this function instead of wprintf() when a va_list is used, rather 
 *		than a variable list of parameters in the argument list.
  *	\param	fmt	The string to be processed. Conforms to normal printf() 
 *		specifications.
 *	\param	ptr	A va_list of the arguments that control the output.
 *	\return	The total number of characters output.
 */
int vprintf(const char* fmt, va_list ptr)
{
	int ret;
#ifdef BUFFER
	if (ch == NULL)
		ch = printf_buffer;
#endif
	ret = doprintf(kprintfhelp, NULL, fmt, ptr);
/*#ifdef BUFFER
	_cputs(printf_buffer, ch - printf_buffer);
#endif*/
	return ret;
}

/*!	\brief Prints a formatted string to the standard output.
 *
 *	\param	fmt	The string to be processed. Conforms to normal printf() 
 *		specifications.
 *	\param	...	Arguments that control the output.
 *	\return	The total number of characters output.
 */
int printf(const char* fmt, ...)
{
	va_list ptr;
	int ret;

	va_start(ptr, fmt);
	ret = vprintf(fmt, ptr);
	va_end(ptr);
	return ret;
}
