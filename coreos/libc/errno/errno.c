/* $Id: errno.c,v 1.1.1.1 2002/12/21 09:50:04 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <errno.h>
#include <os/rtl.h>

/*static int _errno;*/

int *_geterrno()
{
	return &ThrGetThreadInfo()->status;
}