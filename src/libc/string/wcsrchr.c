/* $Id: wcsrchr.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

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