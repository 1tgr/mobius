/* $Id: fwprintf.c,v 1.1 2002/03/27 22:14:48 pavlovskii Exp $ */

#include <stddef.h>
#include <printf.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

static bool kprintfhelp(void* pContext, const wchar_t* str, size_t len)
{
	return fwrite(str, sizeof(wchar_t), len, pContext) == len ? true : false;
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
int vfwprintf(FILE *iop, const wchar_t *fmt, va_list ptr)
{
  int len;
  char localbuf[BUFSIZ];

  if (iop->_flag & _IONBF)
  {
    iop->_flag &= ~_IONBF;
    iop->_ptr = iop->_base = localbuf;
    iop->_bufsiz = BUFSIZ;
    len = dowprintf(kprintfhelp, iop, fmt, ptr);
    fflush(iop);
    iop->_flag |= _IONBF;
    iop->_base = NULL;
    iop->_bufsiz = NULL;
    iop->_cnt = 0;
  }
  else
    len = dowprintf(kprintfhelp, iop, fmt, ptr);
  return ferror(iop) ? EOF : len;
}

/*!	\brief Prints a formatted string to the standard output.
 *
 *	\param	fmt	The string to be processed. Conforms to normal printf() 
 *		specifications.
 *	\param	...	Arguments that control the output.
 *	\return	The total number of characters output.
 */
int fwprintf(FILE *iop, const wchar_t *fmt, ...)
{
	va_list ptr;
	int ret;

	va_start(ptr, fmt);
	ret = vfwprintf(iop, fmt, ptr);
	va_end(ptr);
	return ret;
}
