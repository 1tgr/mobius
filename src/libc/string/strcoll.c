/* $Id: strcoll.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <string.h>

int
strcoll(const char *s1, const char *s2)
{
  return strcmp(s1, s2);
}
