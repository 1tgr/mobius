#include <stdlib.h>
#include <os/os.h>

/*
 * mainCRTStartup
 *
 * The entry point for applications linked to libc.dll.
 *	Initializes libc and calls the program's main, then terminates the process.
 */

int main();

void mainCRTStartup()
{
	exit(main());
}