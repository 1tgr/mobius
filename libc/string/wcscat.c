/* $Id: wcscat.c,v 1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <string.h>

wchar_t *wcscat(wchar_t* dest, const wchar_t* src)
{
	const wchar_t* ret = dest;

	while (*dest)
		dest++;

	while (*src)
		*dest++ = *src++;

	*dest = 0;
	return (wchar_t*) ret;
}