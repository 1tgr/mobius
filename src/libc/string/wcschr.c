/* $Id: wcschr.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <string.h>

wchar_t* wcschr(const wchar_t* str, int c)
{
	for (; *str; str++)
		if (*str == c)
			return (wchar_t*) str;

	return NULL;
}