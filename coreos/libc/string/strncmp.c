/* $Id: strncmp.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <string.h>

int
strncmp(const char *s1, const char *s2, size_t n)
{

  if (n == 0)
    return 0;
  do {
    if (*s1 != *s2++)
      return *(unsigned const char *)s1 - *(unsigned const char *)--s2;
    if (*s1++ == 0)
      break;
  } while (--n != 0);
  return 0;
}
