/* $Id: strpbrk.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <string.h>

char *
strpbrk(const char *s1, const char *s2)
{
  const char *scanp;
  int c, sc;

  while ((c = *s1++) != 0)
  {
    for (scanp = s2; (sc = *scanp++) != 0;)
      if (sc == c)
    return (char*) s1 - 1;
  }
  return 0;
}
