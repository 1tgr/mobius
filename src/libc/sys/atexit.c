#include <stdio.h>

int atexit(void (__cdecl *func)(void))
{
	wprintf(L"atexit(%08x)\n", func);
	return -1;
}