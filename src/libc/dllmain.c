#include <os/os.h>

bool libc_init();
void sysException(dword code, addr_t address, addr_t eip);

bool STDCALL DllMain(dword hDllHandle, dword dwReason, void* lpreserved)
{
	if (!libc_init())
	{
#if defined(KERNEL) && 0
		sysException(EXCEPTION_DLLMAIN_FAILED, (dword) "libc.dll", NULL);
#endif
		return false;
	}
	else
		return true;
}
