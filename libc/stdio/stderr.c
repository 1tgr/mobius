/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <libc/file.h>
#include <libc/stdiohk.h>
#include <os/rtl.h>
#include <os/defs.h>

FILE __dj_stderr = {
  0, 0, 0, 0,
  _IOWRT | _IONBF,
  0
};

FILE *__get_stderr(void)
{
	if (__dj_stderr._file == 0)
		__dj_stderr._file = ProcGetProcessInfo()->std_out;
	return &__dj_stderr;
}
