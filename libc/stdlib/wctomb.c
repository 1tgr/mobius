/* $Id: wctomb.c,v 1.1 2002/12/21 09:50:17 pavlovskii Exp $ */

#include <stdlib.h>

int wctomb(char *s, wchar_t w)
{
    wchar_t wc[2] = { w, '\0' };
    return wcstombs(s, wc, MB_CUR_MAX);
}
