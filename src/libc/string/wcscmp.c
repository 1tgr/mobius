/* $Id: wcscmp.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <string.h>

int wcscmp(const wchar_t *str1, const wchar_t *str2)
{
	while((*str2 != L'\0') && (*str1 == *str2))
	{
		str1++;
		str2++;
	}
	return *str1 -  *str2;
}
