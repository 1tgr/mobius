/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>

void
_pwerror(const wchar_t *s)
{
  fprintf(stderr, "%S: %s\n", s, strerror(errno));
}
