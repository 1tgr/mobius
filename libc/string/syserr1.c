/* $Id: syserr1.c,v 1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */

#define ERRNO(n, s) const char __syserr##n[] = s;
#include "syserr1.h"

