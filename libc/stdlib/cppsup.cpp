/* $Id: cppsup.cpp,v 1.3 2003/06/22 15:46:11 pavlovskii Exp $ */

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

extern "C" void *__cxa_pure_virtual(void)
{
    __asm__("int3");
    abort();
}
