/* $Id: cputws.c,v 1.5 2002/02/27 18:33:55 pavlovskii Exp $ */

#include <string.h>
#include <stdlib.h>

int _cputs(const char *str, size_t count);

int _cputws(const wchar_t *str, size_t len)
{
	char *mb;
	int ret;

	mb = malloc(len);
	len = wcstombs(mb, str, len);
	if (len == -1)
		ret = 0;
	else
		ret = _cputs(mb, len);
	free(mb);
	return ret;
}
