#define INITGUID
#include <stdio.h>
#include <os/os.h>
#include <os/filesys.h>
#include <conio.h>

int main(int argc, char** argv)
{
	IStream* strm;
	char buf[] = "Hello";

	_cputws(L"Opening /devices/serial0...");
	strm = (IStream*) fsOpen(L"/devices/serial0", &IID_IStream);
	if (!strm)
	{
		_cputws(L"Failed to open devices/serial0\n");
		return 1;
	}

	_cputws(L"done\n");
	//while (!_kbhit())
		IStream_Write(strm, buf, sizeof(buf));
	
	IUnknown_Release(strm);
	return 0;
}