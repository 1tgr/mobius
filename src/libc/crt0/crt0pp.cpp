#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>

void* operator new(size_t size)
{
	return malloc(size);
}

void operator delete(void* buf)
{
	free(buf);
}

int __cdecl _purecall()
{
	dword *frame, *obj, *vtbl;
	int i;

	__asm mov frame, ebp
	obj = (dword*) frame[2];
	vtbl = (dword*) *obj;
	wprintf(L"purecall obj = %x, vtbl = %x\n", obj, vtbl);
	for (i = 0; i < 7; i++)
		wprintf(L"\t%x\n", vtbl[i]);

	//exit(1);
	return *((int*) 0);
}
