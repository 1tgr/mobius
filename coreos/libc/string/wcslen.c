/* $Id: wcslen.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <wchar.h>

size_t wcslen(const wchar_t *str)
{
	size_t count;

	for (count = 0; *str; str++, count++)
		;

	return count;
}