/* $Id: morecore.c,v 1.2 2002/12/18 23:13:31 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/defs.h>

void *__morecore(size_t nbytes)
{
    return VmmAlloc(PAGE_ALIGN_UP((int) nbytes) / PAGE_SIZE, 
        0, 
        VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
}
