/* $Id$ */

#include <setjmp.h>

int setjmp(jmp_buf buf)
{
	unsigned long *stack;

	__asm__("mov %%esi, %0\n"
		"mov %%edi, %1\n"
		"pushfl\n"
		"pop %2"
		: "=g" (buf[__esi]), "=g" (buf[__edi]), "=b" (buf[__eflags]));

	stack = (unsigned long*) &buf;
	buf[__eip] = buf[-1];
	buf[__ebp] = buf[-2];
	buf[__ebx] = buf[-3];
	buf[__esp] = (unsigned long) (buf - 2);
	return 0;
}
