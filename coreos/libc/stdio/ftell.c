/* Copyright (C) 1996 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <libc/stubs.h>
#include <stdio.h>
#include <libc/file.h>
#include <os/syscall.h>

long
ftell(FILE *f)
{
  int adjust=0;

  if (f->_cnt < 0)
    f->_cnt = 0;
  if (f->_flag&_IOREAD)
  {
    adjust = - f->_cnt;
  }
  else if (f->_flag&(_IOWRT|_IORW))
  {
    if (f->_flag&_IOWRT && f->_base && (f->_flag&_IONBF)==0)
      adjust = f->_ptr - f->_base;
  }
  else
    return -1;
  return f->_offset + adjust;
}
