/* $Id: getpid.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <os/rtl.h>
#include <os/defs.h>

pid_t getpid(void)
{
	return ProcGetProcessInfo()->id;
}
