/* $Id: getpid.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <unistd.h>
#include <os/rtl.h>
#include <os/defs.h>

pid_t getpid(void)
{
	return ProcGetProcessInfo()->id;
}
