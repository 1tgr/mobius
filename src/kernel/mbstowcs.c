#include <string.h>

size_t mbstowcs(wchar_t *wcstr, const char *mbstr, size_t count)
{
	int i;

	for (i = 0; *mbstr && i < count; i++)
		*wcstr++ = *mbstr++;

	*wcstr = 0;
	return i;
}
