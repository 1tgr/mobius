/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <libc/file.h>
#include <libc/stdiohk.h>
#include <os/rtl.h>
#include <os/defs.h>

FILE *__get_stderr(void)
{
	return __get_stdout();
}
