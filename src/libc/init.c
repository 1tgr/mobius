#include <stdlib.h>
#include <os/os.h>

//! WTF is this?
void __main()
{
}

bool libc_init()
{
	return true;
}

void libc_exit()
{
}

void exit(int status)
{
	procExit();
}