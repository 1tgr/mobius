/* $Id$ */
#ifndef __SETJMP_H
#define __SETJMP_H

typedef unsigned long jmp_buf[7];

enum
{
	__esi,
	__edi,
	__esp,
	__ebp,
	__ebx,
	__eip,
	__eflags
};

void	longjmp(jmp_buf _jmpb, int _retval);
int		setjmp(jmp_buf _jmpb);

#endif
