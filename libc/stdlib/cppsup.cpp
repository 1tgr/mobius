/* $Id: cppsup.cpp,v 1.2 2003/06/05 22:02:12 pavlovskii Exp $ */

#include <stdlib.h>

__attribute__((dllexport)) void *operator new(size_t size)
{
    return malloc(size);
}

__attribute__((dllexport)) void operator delete(void *p)
{
    free(p);
}

__attribute__((dllexport)) void *operator new[](size_t size)
{
    return malloc(size);
}

__attribute__((dllexport)) void operator delete[](void *p)
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
