/* $Id: syscall.S,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

	.file "syscall.S"
	.text
	
#define SYSCALL(rtn, name, argbytes, args...) \
	.globl _##name ;\
_##name: \
	mov $SYS_##name, %eax ;\
	mov $argbytes, %ecx; \
	lea 4(%esp), %edx; \
	int $0x30 ;\
	ret ; \
	.align 8 ;

#include <os/sysdef.h>
