#include <string.h>
#include <stdlib.h>
#include <os/os.h>

wchar_t errorbuf[50];

wchar_t* _wcserror(int errcode)
{
	if (sysGetErrorText(ERRNO_TO_HRESULT(errcode), errorbuf, countof(errorbuf)))
		return errorbuf;
	else
		return L"unknown error";
}