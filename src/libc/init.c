#include <stdlib.h>
#include <os/os.h>
#include <stdio.h>

FILE __iobuf[3];

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
	for (;;)
		procExit();
}

void abort()
{
	exit(EXIT_FAILURE);
}