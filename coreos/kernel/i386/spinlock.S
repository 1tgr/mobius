/*
 * $History: spinlock.S $
; 
; *****************  Version 2  *****************
; User: Tim          Date: 22/02/04   Time: 14:08
; Updated in $/coreos/kernel/i386
; Extended KeAtomicInc/KeAtomicDec to return old value
; Added history block
 */

	.text

	.globl	_SpinAcquire
_SpinAcquire:
	push	%ebp
	mov		%esp, %ebp

	mov		8(%ebp), %ecx
	mov		(%ecx), %eax
	test	%eax, %eax
	jnz		acquire_recursive

	pushfl
	pop		%eax
	mov		%eax, 8(%ecx)

	cli

acquire_loop:
	mov		$1, %eax
	xchg	(%ecx), %eax
	or		%eax, %eax
	jnz		acquire_loop

	mov		4(%ebp), %eax
	mov		%eax, 4(%ecx)

	mov		%ebp, %esp
	pop		%ebp
	ret

acquire_recursive:
	mov		4(%ecx), %eax
	int3
	mov		%ebp, %esp
	pop		%ebp
	ret

	.align 8

	.globl	_SpinRelease
_SpinRelease:
	mov		4(%esp), %eax
	movl	$0, (%eax)
	mov		8(%eax), %eax
	push	%eax
	popfl
	ret

	.align 8

// int KeAtomicInc(int *);
// returns old value
	.globl	_KeAtomicInc
_KeAtomicInc:
	movl 4(%esp),%ecx
	movl $1,%eax
	lock
	xadd %eax,(%ecx)
	ret

	.align 8

// int KeAtomicDec(int *);
// returns old value
	.globl	_KeAtomicDec
_KeAtomicDec:
	movl 4(%esp),%ecx
	movl $-1,%eax
	lock
	xadd %eax,(%ecx)
	ret

	.align 8

	.globl	_i386DoCall
_i386DoCall:
	push	%ebp
	mov		%esp, %ebp

	push	%esi
	push	%edi

	mov		8(%ebp), %eax
	mov		12(%ebp), %esi
	mov		16(%ebp), %ecx

	sub		%ecx, %esp
	mov		%esp, %edi
	rep
	movsb

	call	*%eax

	add		16(%ebp), %esp
	pop		%edi
	pop		%esi

	pop		%ebp
	ret
