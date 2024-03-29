/* $Id: memcmp.c,v 1.1.1.1 2002/12/21 09:50:17 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <string.h>

int
memcmp(const void *s1, const void *s2, size_t n)
{
  if (n != 0)
  {
    const unsigned char *p1 = s1, *p2 = s2;

    do {
      if (*p1++ != *p2++)
	return (*--p1 - *--p2);
    } while (--n != 0);
  }
  return 0;
}
