/* $Id: exit.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <stdlib.h>
#include <os/syscall.h>

void exit(int code)
{
	while (1)
		ProcExitProcess(code);
}