/* $Id: fprintf.c,v 1.1 2002/12/21 09:50:07 pavlovskii Exp $ */

#include <stddef.h>
#include <printf.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

int _cputs(const char *, size_t);

static bool kprintfhelp(void* pContext, const char* str, size_t len)
{
	return fwrite(str, 1, len, pContext) == len ? true : false;
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
int vfprintf(FILE *iop, const char *fmt, va_list ptr)
{
  int len;
  char localbuf[BUFSIZ];

  if (iop->_flag & _IONBF)
  {
    iop->_flag &= ~_IONBF;
    iop->_ptr = iop->_base = localbuf;
    iop->_bufsiz = BUFSIZ;
	len = doprintf(kprintfhelp, iop, fmt, ptr);
    fflush(iop);
    iop->_flag |= _IONBF;
    iop->_base = NULL;
    iop->_bufsiz = NULL;
    iop->_cnt = 0;
  }
  else
    len = doprintf(kprintfhelp, iop, fmt, ptr);
  return ferror(iop) ? EOF : len;
}

/*!	\brief Prints a formatted string to the standard output.
 *
 *	\param	fmt	The string to be processed. Conforms to normal printf() 
 *		specifications.
 *	\param	...	Arguments that control the output.
 *	\return	The total number of characters output.
 */
int fprintf(FILE *iop, const char *fmt, ...)
{
	va_list ptr;
	int ret;

	va_start(ptr, fmt);
	ret = vfprintf(iop, fmt, ptr);
	va_end(ptr);
	return ret;
}
