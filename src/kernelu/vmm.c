#include <os/os.h>

void* vmmAlloc(size_t pages, addr_t start, dword flags)
{
	void* ret;
	asm("int $0x30" 
		: "=a" (ret) 
		: "a" (0x300), "b" (pages), "c" (start), "d" (flags));
	return ret;
}