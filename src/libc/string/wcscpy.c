/* $Id: wcscpy.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <string.h>

wchar_t *wcscpy(wchar_t *strDestination, const wchar_t *strSource)
{
	wchar_t* dest = strDestination;

	while (*strSource)
		*strDestination++ = *strSource++;

	*strDestination = 0;
	return dest;
}