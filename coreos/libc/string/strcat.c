/* $Id: strcat.c,v 1.1.1.1 2002/12/21 09:50:17 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <string.h>

char *
strcat(char *s, const char *append)
{
  char *save = s;

  for (; *s; ++s);
  while ((*s++ = *append++));
  return save;
}
