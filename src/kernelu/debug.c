#include <os/device.h>
#include <os/os.h>
#include <wchar.h>

int dputws(const wchar_t* str)
{
	addr_t debugger;
	request_t dreq;

	debugger = devOpen(L"debugger", NULL);

	dreq.header.code = DEV_WRITE;
	dreq.params.write.length = sizeof(wchar_t) * wcslen(str);
	dreq.params.write.buffer = str;
	if (devUserRequest(debugger, &dreq, sizeof(dreq)))
		thrWaitHandle((addr_t*) &dreq.header.event, 1, true);

	devUserFinishRequest(&dreq, true);
	devClose(debugger);
	return 0;
}