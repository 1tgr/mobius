/* $Id: wcserror.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <wchar.h>
#include <errno.h>

wchar_t *
_wcserror(int errnum)
{
  static wchar_t ebuf[40];		/* 64-bit number + slop */
  wchar_t *cp;
  int v=1000000, lz=0;

  if (errnum >= 0 && errnum < __sys_nerr)
    return sys_werrlist[errnum];

  wcscpy(ebuf, L"Unknown error: ");
  cp = ebuf + 15;
  if (errnum < 0)
  {
    *cp++ = '-';
    errnum = -errnum;
  }
  while (v)
  {
    int d = errnum / v;
    if (d || lz || (v == 1))
    {
      *cp++ = d+'0';
      lz = 1;
    }
    errnum %= v;
    v /= 10;
  }

  return ebuf;
}
