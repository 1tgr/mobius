/* $Id: wcsrchr.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <string.h>

wchar_t* wcsrchr(const wchar_t* str, int c)
{
	const wchar_t *start = str;
	while (*str)
		str++;

	for (; str >= start; str--)
		if (*str == c)
			return (wchar_t*) str;

	return NULL;
}