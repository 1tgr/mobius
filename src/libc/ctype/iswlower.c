/* $Id: iswlower.c,v 1.1 2002/05/12 00:30:33 pavlovskii Exp $ */

#include <wchar.h>

#undef iswlower

int iswlower(wint_t c)
{
    if (c < 0)
        return 0;
    else
        return c == towlower(c);
}
