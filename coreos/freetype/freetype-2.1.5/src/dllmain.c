/* $Id: dllmain.c,v 1.1.1.1 2002/12/21 09:50:26 pavlovskii Exp $ */

#include <os/defs.h>

bool DllMainCRTStartup(module_info_t *mod, unsigned reason)
{
	return true;
}
