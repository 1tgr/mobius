/* $Id: stricmp.c,v 1.3 2002/06/09 18:36:41 pavlovskii Exp $ */

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
