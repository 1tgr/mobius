/* $Id: sprintf.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <printf.h>
#include <stdio.h>

static bool sprintfhelp(void* pContext, const char* str, size_t len)
{
	char** buffer = (char**) pContext;
	strcpy(*buffer, str);
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
int vsprintf(char *buffer, const char *format, va_list argptr)
{
	return doprintf(sprintfhelp, &buffer, format, argptr);
}

/*!	\brief Copies a formatted string to a buffer.
 *	\param	fmt	The string to be processed. Conforms to normal wprintf() 
 *		specifications.
 *	\param	...	Arguments that control the output.
 *	\return	The total number of characters copied.
 */
int sprintf(char *buffer, const char *format, ...)
{
	va_list ptr;
	int ret;

	va_start(ptr, format);
	ret = vsprintf(buffer, format, ptr);
	va_end(ptr);
	return ret;
}