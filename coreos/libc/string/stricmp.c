/* $Id: stricmp.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <string.h>
#include <ctype.h>

int _stricmp(const char *str1, const char*str2)
{
    while ((*str2 != L'\0') && (tolower(*str1) == tolower(*str2)))
    {
	str1++;
	str2++;
    }

    return tolower(*str1) -  tolower(*str2);
}
