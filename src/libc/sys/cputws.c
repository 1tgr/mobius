/* $Id: cputws.c,v 1.6 2002/03/04 18:56:32 pavlovskii Exp $ */

#include <string.h>
#include <stdlib.h>

int _cputs(const char *str, size_t count);

int _cputws(const wchar_t *str, size_t len)
{
    char *mb;
    
    /*
     * xxx - this doesn't handle multi-byte sequences properly, because str
     *	isn't necessarily nul-terminated.
     */
    len = wcstombs(NULL, str, len * MB_CUR_MAX);
    if (len == (size_t) -1)
	return -1;

    mb = malloc(len);
    wcstombs(mb, str, len);
    _cputs(mb, len);
    free(mb);
    return 0;
}
