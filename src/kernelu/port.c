#include <os/port.h>

addr_t portCreate(const wchar_t* name)
{
	addr_t ret;
	asm("int $0x30": "=a" (ret): "a" (0x800), "b" (name));
	return ret;
}

bool portListen(addr_t port)
{
	bool ret;
	asm("int $0x30": "=a" (ret): "a" (0x801), "b" (port));
	return ret;
}

bool portConnect(addr_t port, const wchar_t* remote)
{
	bool ret;
	asm("int $0x30": "=a" (ret): "a" (0x802), "b" (port), "c" (remote));
	return ret;
}

addr_t portAccept(addr_t server)
{
	addr_t ret;
	asm("int $0x30": "=a" (ret): "a" (0x803), "b" (server));
	return ret;
}

bool portClose(addr_t port)
{
	bool ret;
	asm("int $0x30": "=a" (ret): "a" (0x804), "b" (port));
	return ret;
}