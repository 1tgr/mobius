#include <wchar.h>

long disasm (unsigned char *data, wchar_t *output, int segsize, long offset,
	     int autosync, unsigned long prefer)
{
	wcscpy(output, L"(disassembler not loaded)");
	return 0;
}

void init_sync(void)
{
}