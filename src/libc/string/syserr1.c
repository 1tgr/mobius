/* $Id: syserr1.c,v 1.3 2002/03/27 22:13:01 pavlovskii Exp $ */

/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */

#define ERRNO(n, s) const char __syserr##n[] = s;
#include "syserr1.h"

