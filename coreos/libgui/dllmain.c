/* $Id: dllmain.c,v 1.1.1.1 2002/12/21 09:50:26 pavlovskii Exp $ */

#include <os/defs.h>
#include <os/rtl.h>
#include <os/syscall.h>

#include <gui/winmgr.h>
#include <gui/ipc.h>
#include "internal.h"

wmgr_global_t *wmgr_global;

static bool GuiInit(void)
{
	handle_t desc;

	ThrGetThreadInfo()->exception_handler = 0;
	if (!WndConnectIpc())
		return false;

	desc = HndOpen(WMGR_GLOBAL_NAME);
	if (desc == 0)
	{
		WndDisconnectIpc();
		return false;
	}

	wmgr_global = VmmMapSharedArea(desc, 0, VM_MEM_USER | VM_MEM_READ);
	HndClose(desc);
	if (wmgr_global == NULL)
	{
		WndDisconnectIpc();
		return false;
	}

	return true;
}


static void GuiCleanup(void)
{
	VmmFree(wmgr_global);
	WndDisconnectIpc();
}


bool DllMainCRTStartup(module_info_t *mod, unsigned reason)
{
	switch (reason)
	{
	case DLLMAIN_PROCESS_STARTUP:
	case DLLMAIN_THREAD_STARTUP:
		return GuiInit();

	case DLLMAIN_PROCESS_EXIT:
	case DLLMAIN_THREAD_EXIT:
		GuiCleanup();
		break;
	}

	return true;
}
