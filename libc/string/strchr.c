/* $Id: strchr.c,v 1.1 2002/12/21 09:50:17 pavlovskii Exp $ */

/* Copyright (C) 1996 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <string.h>

char *
strchr(const char *s, int c)
{
  char cc = c;
  while (*s)
  {
    if (*s == cc)
      return (char*) s;
    s++;
  }
  if (cc == 0)
    return (char*) s;
  return 0;
}

