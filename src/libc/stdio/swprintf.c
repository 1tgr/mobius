#include <printf.h>
#include <wchar.h>

static bool swprintfhelp(void* pContext, const wchar_t* str, size_t len)
{
	wchar_t** buffer = (wchar_t**) pContext;
	wcscpy(*buffer, str);
	*buffer += len;
	return true;
}

/*!	\brief Copies a formatted string to a buffer, given a va_list of arguments.
 *	Use this function instead of swprintf() when a va_list is used, rather 
 *		than a variable list of parameters in the argument list.
  *	\param	fmt	The string to be processed. Conforms to normal wprintf() 
 *		specifications.
 *	\param	ptr	A va_list of the arguments that control the output.
 *	\return	The total number of characters copied.
 */
int vswprintf(wchar_t *buffer, const wchar_t *format, va_list argptr)
{
	return dowprintf(swprintfhelp, &buffer, format, argptr);
}

/*!	\brief Copies a formatted string to a buffer.
 *	\param	fmt	The string to be processed. Conforms to normal wprintf() 
 *		specifications.
 *	\param	...	Arguments that control the output.
 *	\return	The total number of characters copied.
 */
int swprintf(wchar_t *buffer, const wchar_t *format, ...)
{
	va_list ptr;
	int ret;

	va_start(ptr, format);
	ret = vswprintf(buffer, format, ptr);
	va_end(ptr);
	return ret;
}