/* $Id: dllmain.c,v 1.1.1.1 2002/12/21 09:49:35 pavlovskii Exp $ */

#include <os/defs.h>
#include <libc/local.h>

bool DllMainCRTStartup(module_info_t *mod, unsigned reason)
{
	switch (reason)
	{
	case DLLMAIN_PROCESS_STARTUP:
		__libc_init();
		break;
	}

	return true;
}
