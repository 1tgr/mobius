/* $Id: wcscpy.c,v 1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <string.h>

wchar_t *wcscpy(wchar_t *strDestination, const wchar_t *strSource)
{
	wchar_t* dest = strDestination;

	while (*strSource)
		*strDestination++ = *strSource++;

	*strDestination = 0;
	return dest;
}