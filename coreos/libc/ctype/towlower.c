/* $Id: towlower.c,v 1.1.1.1 2002/12/21 09:49:36 pavlovskii Exp $ */

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
