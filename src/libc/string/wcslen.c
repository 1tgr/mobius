#include <string.h>

size_t wcslen(const wchar_t* str)
{
	size_t len = 0;

	while (*str)
	{
		len++;
		str++;
	}

	return len;
}