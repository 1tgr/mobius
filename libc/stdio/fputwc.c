/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <libc/file.h>
#include <wchar.h>
#include <stdlib.h>

wint_t fputwc(wchar_t c, struct FILE *fp)
{
  char ch[5];
  wctomb(ch, c);
  if (fputs(ch, fp) == 0)
    return c;
  else
    return WEOF;
}
