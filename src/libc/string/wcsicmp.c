#include <string.h>

#define isupper(ch)	((ch) >= L'A' && (ch) <= L'Z')
#define tolower(ch)	(isupper(ch) ? (ch) - L'A' + L'a' : (ch))

int wcsicmp(const wchar_t *str1, const wchar_t *str2)
{
	while((*str2 != L'\0') && (tolower(*str1) == tolower(*str2)))
	{
		str1++;
		str2++;
	}
	return *str1 -  *str2;
}
