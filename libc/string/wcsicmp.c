/* $Id: wcsicmp.c,v 1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <wchar.h>

int _wcsicmp(const wchar_t *str1, const wchar_t *str2)
{
    while ((*str2 != L'\0') && (towlower(*str1) == towlower(*str2)))
    {
	str1++;
	str2++;
    }

    return towlower(*str1) - towlower(*str2);
}
