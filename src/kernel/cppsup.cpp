/* $Id: cppsup.cpp,v 1.4 2002/08/14 16:23:59 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>

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
