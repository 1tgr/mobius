/* $Id: syswerr2.c,v 1.1 2002/03/27 22:15:59 pavlovskii Exp $ */

/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <wchar.h>
#include <errno.h>

#define ERRNO(n, s) extern wchar_t __syswerr##n[];
#include "syserr1.h"

#undef ERRNO
#define ERRNO(n, s) __syswerr##n, 

const wchar_t *__sys_werrlist[] = {
#include "syserr1.h"
};
