/* Copyright (C) 1999 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <libc/file.h>
#include <wchar.h>
#include <stdlib.h>

int
fputws(const wchar_t *s, FILE *f)
{
  int r = 0;
  char c;
  int unbuffered;
  char localbuf[BUFSIZ];
  wchar_t wc;
  
  unbuffered = f->_flag & _IONBF;
  if (unbuffered)
  {
    f->_flag &= ~_IONBF;
    f->_ptr = f->_base = localbuf;
    f->_bufsiz = BUFSIZ;
  }

  while ((wc = *s++))
  {
    wctomb(&c, wc);
    if ((r = __putc(c, f)) == EOF)
      break;
  }

  if (unbuffered)
  {
    if (fflush(f) == EOF)
      r = EOF;
    f->_flag |= _IONBF;
    f->_base = NULL;
    f->_bufsiz = NULL;
    f->_cnt = 0;
  }

  return(r);
}
