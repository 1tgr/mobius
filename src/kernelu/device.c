#include <os/device.h>

addr_t devOpen(const wchar_t* name, const wchar_t* params)
{
	addr_t r;
	asm("int $0x30" : "=a" (r) : "a" (0x500), "b" (name), "c" (params));
	return r;
}

bool devClose(addr_t dev)
{
	bool r;
	asm("int $0x30" : "=a" (r) : "a" (0x501), "b" (dev));
	return r;
}

bool devUserRequest(addr_t dev, request_t* req, size_t size)
{
	bool r;
	asm("int $0x30" : "=a" (r) : "a" (0x502), "b" (dev), "c" (req), "d" (size));
	return r;
}

void devUserFinishRequest(request_t* req, bool delete_event)
{
	asm("int $0x30" : : "a" (0x503), "b" (req), "c" (delete_event));
}