/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void	__dj_assert(const char *msg, const char *file, int line)
{
  /* Assertion failed at foo.c line 45: x<y */
  _wdprintf(L"Assertion failed at %S line %d: %S\n", file, line, msg);
  /*raise(SIGABRT);*/
  __asm__("int3");
}
