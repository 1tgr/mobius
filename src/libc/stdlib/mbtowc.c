/* $Id: mbtowc.c,v 1.3 2002/08/04 16:06:05 pavlovskii Exp $ */

#include <stdlib.h>

int mbtowc(wchar_t *pwc, const char *s, size_t n)
{
    wchar_t wc[2];
    int ret;
    ret = mbstowcs(wc, s, n);
    *pwc = wc[0];
    return ret;
}
