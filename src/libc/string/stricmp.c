/* $Id: stricmp.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <string.h>

#define isupper(ch)	((ch) >= 'A' && (ch) <= 'Z')
#define tolower(ch)	(isupper(ch) ? (ch) - 'A' + 'a' : (ch))

int _stricmp(const char *str1, const char*str2)
{
	while((*str2 != L'\0') && (tolower(*str1) == tolower(*str2)))
	{
		str1++;
		str2++;
	}
	return *str1 -  *str2;
}
