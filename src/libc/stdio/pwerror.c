/* $Id: pwerror.c,v 1.1 2002/03/05 02:10:21 pavlovskii Exp $ */

#include <os/rtl.h>

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>

void _pwerror(const wchar_t *text)
{
    wchar_t str[1024];
    int en;

    en = errno;
    if (en < 0)
	en = 0;

    if (!ResLoadString(NULL, en + 1024, str, _countof(str)))
	swprintf(str, L"errno = %d", en);

    wprintf(L"%s: %s\n", text, str);	    
}
