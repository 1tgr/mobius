/* $Id: syscall.S,v 1.1 2002/09/13 23:26:02 pavlovskii Exp $ */

    .file "syscall.S"
    .text
    
#define SYSCALL_GUI(rtn, name, argbytes, args...) \
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

#include <gui/sysdef.h>
