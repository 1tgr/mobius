/* $Id: syserr2.c,v 1.3 2002/03/27 22:13:01 pavlovskii Exp $ */

/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <errno.h>

#define ERRNO(n, s) extern char __syserr##n[];
#include "syserr1.h"

#undef ERRNO
#define ERRNO(n, s) __syserr##n, 

const char *__sys_errlist[] = {
#include "syserr1.h"
};

int __sys_nerr = sizeof(__sys_errlist) / sizeof(__sys_errlist[0]);
