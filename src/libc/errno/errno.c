/* $Id: errno.c,v 1.4 2002/02/25 18:42:09 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <errno.h>
#include <os/rtl.h>

/*static int _errno;*/

int *_geterrno()
{
	return &ThrGetThreadInfo()->status;
}