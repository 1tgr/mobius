/* $Id: wcsdup.c,v 1.3 2002/05/12 00:13:20 pavlovskii Exp $ */

#include <wchar.h>
#include <stdlib.h>

wchar_t *_wcsdup(const wchar_t* str)
{
    wchar_t *ret;
    
    if (str == NULL)
	    str = L"";

    ret = malloc((wcslen(str) + 1) * sizeof(wchar_t));

    if (!ret)
	    return NULL;

    wcscpy(ret, str);
    return ret;
}
