/* $Id: wcslen.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <wchar.h>

size_t wcslen(const wchar_t *str)
{
	size_t count;

	for (count = 0; *str; str++, count++)
		;

	return count;
}