/* $Id: wctomb.c,v 1.4 2002/08/04 16:06:05 pavlovskii Exp $ */

#include <stdlib.h>

int wctomb(char *s, wchar_t w)
{
    wchar_t wc[2] = { w, '\0' };
    return wcstombs(s, wc, MB_CUR_MAX);
}
