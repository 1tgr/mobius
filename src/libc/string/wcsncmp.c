/* $Id: wcsncmp.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <wchar.h>

int
wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{

  if (n == 0)
    return 0;
  do {
    if (*s1 != *s2++)
      return *s1 - *--s2;
    if (*s1++ == 0)
      break;
  } while (--n != 0);
  return 0;
}
