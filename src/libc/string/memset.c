#include <string.h>

void *memset(void *dest, int c, size_t count)
{
	char* ch;

	for (ch = (char*) dest; count; count--)
		*ch++ = c;

	return dest;
}