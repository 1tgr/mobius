[bits 32]
[section .text]

[global _KernelEntry]
[global _kernel_startup]

[extern _ArchStartup]
[extern _kernel_stack_end]
[extern _scode]
[extern _sbss]
[extern _ebss]

_KernelEntry:
	;            eax -> address at top of memory
	;            ebx -> size of kernel
	;            ecx -> physical address of start
	;            edx -> size of ramdisk
	;            esi -> physical address of ramdisk
	; cs, ds, es, ss -> base = start
	;             fs -> base = start of loader (.COM program)
	;             gs -> linear (base = 0)

	mov	esp, _kernel_stack_end
	sub	esp, _scode

	mov	ebp, _kernel_startup
	sub	ebp, _scode
	mov	[ebp + 0], eax
	mov	[ebp + 4], ebx
	mov	[ebp + 8], ecx
	mov	[ebp + 12], edx
	mov	[ebp + 16], esi

	mov	edi, _sbss
	mov	ecx, _ebss
	sub	ecx, edi
	sub	edi, _scode
	xor	eax, eax
	rep stosd

	push	ebp
	call	_ArchStartup
	jmp	$

[section .data]
_kernel_startup	times 6 dd 0