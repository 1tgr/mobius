/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <libc/file.h>
#include <wchar.h>
#include <stdlib.h>

wint_t
getwc(FILE *f)
{
    int c;
    char str[4];
    wchar_t wc;

    c = __getc(f);
    if (c == EOF)
        return WEOF;

    if ((c & 0x80) == 0)
        return c;
    else if ((c & 0xE0) == 0xC0)
    {
        str[0] = c;

        c = __getc(f);
        if (c == EOF)
            return WEOF;
        str[1] = c;

        str[2] = '\0';

        if (mbtowc(&wc, str, 2) == -1)
            return WEOF;

        return wc;
    }
    else if ((c & 0xF0) == 0xE0)
    {
        str[0] = c;

        c = __getc(f);
        if (c == EOF)
            return WEOF;
        str[1] = c;

        c = __getc(f);
        if (c == EOF)
            return WEOF;
        str[2] = c;

        str[3] = '\0';

        if (mbtowc(&wc, str, 3) == -1)
            return WEOF;

        return wc;
    }

    return WEOF;
}
