#include <sys/types.h>
#include <os/pe.h>
#include <os/os.h>

addr_t conConnect();
void conClose();

bool __stdcall DllMain(dword hDllHandle, dword dwReason, void* lpreserved)
{
	thread_info_t *thr;
	IMAGE_DOS_HEADER *dos;
	IMAGE_PE_HEADERS *pe;

	thr = thrGetInfo();
	dos = (IMAGE_DOS_HEADER*) thr->process->base;
	pe = (IMAGE_PE_HEADERS*) (thr->process->base + dos->e_lfanew);

	if (dwReason == 0)
	{
		/*if (pe->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_NATIVE)
			return true;
		else
			return conConnect();*/
		return true;
	}
	else
		conClose();

	return true;
}
