#include <os/os.h>

/*#pragma data_seg(".info")
module_info_t __declspec(allocate(".info")) _info;
#pragma data_seg()*/

bool STDCALL DllMainCRTStartup(dword hDllHandle, dword dwReason, void* lpreserved)
{
	/*__asm
	{
		mov eax, [_info]
		mov [_info], eax
	}*/

	return DllMain(hDllHandle, dwReason, lpreserved);
}
