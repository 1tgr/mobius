/* $Id: towlower.c,v 1.1 2002/05/12 00:30:33 pavlovskii Exp $ */

#include <ctype.h>
#include <wchar.h>

#undef towlower

int towlower(wint_t c)
{
    if (c < 0)
        return c;
    else
    {
        const __wchar_info_t *wc;
        wc = __lookup_unicode(c);
        if (wc == NULL)
            return tolower(c);
        else
            return wc->__lc;
    }
}
