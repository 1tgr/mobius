/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <libc/file.h>
#include <libc/stdiohk.h>
#include <os/rtl.h>
#include <os/defs.h>

FILE __dj_stdout = {
  0, 0, 0, 0,
  _IOWRT | _IOFBF,
  0
};

FILE *__get_stdout(void)
{
    if (__dj_stdout._file == 0)
	__dj_stdout._file = ProcGetProcessInfo()->std_out;
    return &__dj_stdout;
}
