/* $Id: towupper.c,v 1.1 2002/12/21 09:49:36 pavlovskii Exp $ */

#include <ctype.h>
#include <wchar.h>

#undef towupper

int towupper(wint_t c)
{
    if (c < 0)
        return c;
    else
    {
        const __wchar_info_t *wc;
        wc = __lookup_unicode(c);
        if (wc == NULL)
            return toupper(c);
        else
            return wc->__uc;
    }
}
