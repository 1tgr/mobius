/* $Id: sbrk.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <sys/types.h>
#include <stddef.h>
#include <os/syscall.h>
#include <os/defs.h>

#define MEM_READ		1
#define MEM_WRITE		2
#define MEM_ZERO		4
#define MEM_COMMIT		8
#define MEM_LITERAL		16

size_t getpagesize(void)
{
	return PAGE_SIZE;
}

char *sbrk(size_t diff)
{
	void *ptr;
	ptr = VmmAlloc(PAGE_ALIGN_UP(diff), NULL, 3 | MEM_READ | MEM_WRITE);

	if (ptr)
		return ptr;
	else
		return (char*) -1;
}