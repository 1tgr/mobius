/* $Id$ */

#include <kernel/addresses.h>

	.text

isr_and_switch:
	push	%ds
	push	%es
	push	%fs
	push	%gs
	pushal

	mov		%cr2, %eax
	push	%eax
	xor		%eax, %eax
	mov		%eax, %cr2

	mov		$KERNEL_FLAT_DATA, %bx
	mov		%ebx, %ds
	mov		%ebx, %es
	mov		%ebx, %gs
	mov		$USER_THREAD_INFO, %bx
	mov		%ebx, %fs

	mov		4 + _thr_cpu_single, %eax
	mov		(%eax), %eax
	push	%eax

	mov		%esp, %ebx
	push	%ebx

	call	_i386Isr

	.globl	_isr_switch_ret
_isr_switch_ret:
	lea		8(%eax), %esp

	popal
	pop		%gs
	pop		%fs
	pop		%es
	pop		%ds
	add		$8, %esp
	iretl

	.align	8


#define exception(n) \
	.globl	_exception_##n ;\
	\
_exception_##n: \
	pushl	$0x##n ;\
	jmp		isr_and_switch ;\
	.align	8


#define irq(n) \
	.globl	_irq_##n ;\
	\
_irq_##n: \
	pushl	$-1 ;\
	pushl	$0x##n; \
	jmp		isr_and_switch ;\
	.align	8
	
	
#define interrupt(n) \
	.globl	_interrupt_##n ;\
	\
_interrupt_##n: \
	pushl	$0 ;\
	pushl	$0x##n; \
	jmp		isr_and_switch ;\
	.align	8
	
	interrupt(0)
	interrupt(1)
	interrupt(2)
	interrupt(3)
	interrupt(4)
	interrupt(5)
	interrupt(6)
	interrupt(7)

	.globl	_exception_8
_exception_8:
	call	_i386DoubleFault
	add		$8, %esp
	iretl
	.align	8

	interrupt(9)
	exception(a)
	exception(b)
	exception(c)
	exception(d)
	exception(e)
	interrupt(f)
	interrupt(10)
	interrupt(11)
	interrupt(12)
	interrupt(13)
	interrupt(14)
	interrupt(15)
	interrupt(16)
	interrupt(17)
	interrupt(18)
	interrupt(19)
	interrupt(1a)
	interrupt(1b)
	interrupt(1c)
	interrupt(1d)
	interrupt(1e)
	interrupt(1f)
	irq(0)
	irq(1)
	irq(2)
	irq(3)
	irq(4)
	irq(5)
	irq(6)
	irq(7)
	irq(8)
	irq(9)
	irq(a)
	irq(b)
	irq(c)
	irq(d)
	irq(e)
	irq(f)
	interrupt(30)
