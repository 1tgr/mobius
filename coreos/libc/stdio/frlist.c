/* Copyright (C) 1999 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <libc/local.h>

static __file_rec __initial_file_rec;
__file_rec *__file_rec_list = &__initial_file_rec;

/* This is done dynamically (called from __crt1_startup) rather than
   statically, because otherwise any program which restarts itself
   (Emacs) will reuse stale FILE objects from the time it was dumped.  */
void
__setup_file_rec_list(void)
{
  __initial_file_rec.next = 0;
  __initial_file_rec.count = 3;
  __initial_file_rec.files[0] = stdin;
  __initial_file_rec.files[1] = stdout;
  __initial_file_rec.files[2] = stderr;
}
