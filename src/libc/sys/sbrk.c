/* $Id: sbrk.c,v 1.5 2002/02/27 18:33:55 pavlovskii Exp $ */

#include <unistd.h>
#include <sys/types.h>
#include <stddef.h>
#include <os/syscall.h>
#include <os/defs.h>

size_t getpagesize(void)
{
	return PAGE_SIZE;
}

char *sbrk(size_t diff)
{
	void *ptr;
	ptr = VmmAlloc(PAGE_ALIGN_UP(diff) / PAGE_SIZE, NULL, 3 | MEM_READ | MEM_WRITE);

	if (ptr)
		return ptr;
	else
		return (char*) -1;
}
