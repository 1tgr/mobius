/* $Id: wcsdup.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <wchar.h>
#include <stdlib.h>

#undef _wcsdup
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
