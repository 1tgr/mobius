/* $Id: morecore.c,v 1.1 2002/05/12 00:30:34 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/defs.h>

void *__morecore(size_t nbytes)
{
    return VmmAlloc(PAGE_ALIGN_UP((int) nbytes) / PAGE_SIZE, 
        0, 
        MEM_READ | MEM_WRITE);
}
