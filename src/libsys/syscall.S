/* $Id: syscall.S,v 1.3 2002/04/20 12:47:28 pavlovskii Exp $ */

    .file "syscall.S"
    .text
    
#define SYSCALL(rtn, name, argbytes, args...) \
    .globl _##name ;\
_##name: \
    push %ebp ;\
    mov %esp, %ebp ;\
    mov $SYS_##name, %eax ;\
    mov $argbytes, %ecx; \
    lea 8(%esp), %edx; \
    int $0x30 ;\
    mov %ebp, %esp ;\
    pop %ebp ;\
    ret ; \
    .align 8 ;

#include <os/sysdef.h>
