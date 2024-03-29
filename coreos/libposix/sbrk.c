/* $Id: sbrk.c,v 1.1.1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

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
	ptr = VmmAlloc(PAGE_ALIGN_UP(diff) / PAGE_SIZE, NULL, 
            VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);

	if (ptr)
		return ptr;
	else
		return (char*) -1;
}
