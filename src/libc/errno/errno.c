/* $Id: errno.c,v 1.3 2002/02/24 19:13:30 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <errno.h>

static int _errno;

int *_geterrno()
{
	return &_errno;
}