/* $Id: wcschr.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <string.h>

wchar_t* wcschr(const wchar_t* str, int c)
{
	for (; *str; str++)
		if (*str == c)
			return (wchar_t*) str;

	return NULL;
}