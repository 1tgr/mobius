#include <stdlib.h>
#include <os/syscall.h>

void exit(int code)
{
	while (1)
		ProcExitProcess(code);
}