/* $Id: syserr3.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <errno.h>

#define ERRNO(n, s) extern char __syserr##n[];
#include "syserr1.h"

#undef ERRNO
#define ERRNO(n, s) __syserr##n, 

char *sys_errlist[] = {
#include "syserr1.h"
};

int sys_nerr = sizeof(sys_errlist) / sizeof(sys_errlist[0]);
