/* $Id: cppsup.cpp,v 1.1 2002/12/21 09:50:10 pavlovskii Exp $ */

#include <stdlib.h>

void *operator new(size_t size)
{
    return malloc(size);
}

void operator delete(void *p)
{
    free(p);
}

void *operator new[](size_t size)
{
    return malloc(size);
}

void operator delete[](void *p)
{
    free(p);
}

extern "C" void *__get_eh_context(void)
{
    static unsigned int temp[2];
    return temp;
}

extern "C" void *__pure_virtual(void)
{
    __asm__("int3");
    abort();
}
