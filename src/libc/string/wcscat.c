/* $Id: wcscat.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

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