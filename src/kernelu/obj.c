#include <os/os.h>

marshal_t objMarshal(void* p)
{
	marshal_t ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x400), "b" (p));
	return ret;
}

void* objUnmarshal(marshal_t mshl)
{
	void* ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x401), "b" (mshl));
	return ret;
}

void objNotifyDelete(marshal_t mshl)
{
	asm("int $0x30" : : "a" (0x402), "b" (mshl));
}