/* $Id: iswlower.c,v 1.1 2002/12/21 09:49:36 pavlovskii Exp $ */

#include <wchar.h>

#undef iswlower

int iswlower(wint_t c)
{
    if (c < 0)
        return 0;
    else
        return c == towlower(c);
}
