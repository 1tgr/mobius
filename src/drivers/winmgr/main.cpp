#include "WindowServer.h"
#include "gfxcons.h"

extern "C" bool STDCALL DllMain(dword hDllHandle, dword dwReason, void* lpreserved)
{
	return true;
}

extern "C" bool __declspec(dllexport) drvInit()
{
	sysExtRegisterDevice(L"aaa", new CWindowServer);
	//sysExtRegisterDevice(L"screen", new CGfxConsole);
	return true;
}