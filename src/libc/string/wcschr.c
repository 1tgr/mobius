#include <string.h>

wchar_t* wcschr(const wchar_t* str, int c)
{
	for (; *str; str++)
		if (*str == c)
			return (wchar_t*) str;

	return NULL;
}