#include <stdlib.h>
#include <printf.h>
#include <string.h>
#include <stdio.h>

static wchar_t printf_buffer[80], *ch;

static bool kprintfhelp(void* pContext, const wchar_t* str, dword len)
{
	if (ch + len > printf_buffer + countof(printf_buffer) ||
		*str == L'\r' || *str == L'\n')
	{
		_cputws(printf_buffer);
		ch = printf_buffer;
	}

	wcscpy(ch, str);
	ch += len;
	return true;
}

//! Prints a formatted string to the standard output, given a va_list of arguments.
/*!
 *	Use this function instead of wprintf() when a va_list is used, rather 
 *		than a variable list of parameters in the argument list.
  *	\param	fmt	The string to be processed. Conforms to normal wprintf() 
 *		specifications.
 *	\param	ptr	A va_list of the arguments that control the output.
 *	\return	The total number of characters output.
 */
int vwprintf(const wchar_t* fmt, va_list ptr)
{
	int ret;
	ch = printf_buffer;
	ret = dowprintf(kprintfhelp, NULL, fmt, ptr);
	_cputws(printf_buffer);
	return ret;
}

//! Prints a formatted string to the standard output.
/*!
 *	\param	fmt	The string to be processed. Conforms to normal wprintf() 
 *		specifications.
 *	\param	...	Arguments that control the output.
 *	\return	The total number of characters output.
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