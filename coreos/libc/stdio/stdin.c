/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <libc/file.h>
#include <libc/stdiohk.h>
#include <os/rtl.h>
#include <os/defs.h>

FILE __dj_stdin = {
  0, 0, 0, 0,
  _IOREAD | _IOLBF,
  0
};

FILE *__get_stdin(void)
{
	if (__dj_stdin._file == 0)
		__dj_stdin._file = ProcGetProcessInfo()->std_in;
	return &__dj_stdin;
}
