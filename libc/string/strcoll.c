/* $Id: strcoll.c,v 1.1 2002/12/21 09:50:17 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <string.h>

int
strcoll(const char *s1, const char *s2)
{
  return strcmp(s1, s2);
}
