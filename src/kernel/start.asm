[bits 32]
[section .text]

[global _KernelEntry]
[global _kernel_startup]

[extern _ArchStartup]
[extern _kernel_stack_end]
[extern _scode]
[extern _sbss]
[extern _ebss]
[extern _edata]

    MULTIBOOT_PAGE_ALIGN   equ 1<<0
    MULTIBOOT_MEMORY_INFO  equ 1<<1
    MULTIBOOT_AOUT_KLUDGE  equ 1<<16

    MULTIBOOT_HEADER_MAGIC equ 0x1BADB002
    MULTIBOOT_HEADER_FLAGS equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_AOUT_KLUDGE
    CHECKSUM               equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
    base                   equ 0xC0000000
    phys                   equ 0x100000

    ; The Multiboot header
    align 4
    mboot:
        dd MULTIBOOT_HEADER_MAGIC
        dd MULTIBOOT_HEADER_FLAGS
        dd CHECKSUM
    ; fields used if MULTIBOOT_AOUT_KLUDGE is set in MULTIBOOT_HEADER_FLAGS
        dd mboot - base	+ phys			; these are PHYSICAL addresses
        dd phys							; start of kernel .text (code) section
        dd _sbss - base + phys			; end of kernel .data section
        dd _ebss - base + phys			; end of kernel BSS
        dd _KernelEntry - base + phys	; kernel entry point (initial EIP)

_KernelEntry:
	;            eax -> address at top of memory
	;            ebx -> size of kernel
	;            ecx -> physical address of start
	;            edx -> size of ramdisk
	;            esi -> physical address of ramdisk
	; cs, ds, es, ss -> base = start
	;             fs -> base = start of loader (.COM program)
	;             gs -> linear (base = 0)

	mov	esp, _kernel_stack_end + phys - base
	;sub	esp, _scode - phys
	
	;mov	ebp, _kernel_startup
	;sub	ebp, _scode
	;mov	[ebp + 0], eax
	;mov	[ebp + 4], ebx
	;mov	[ebp + 8], ecx
	;mov	[ebp + 12], edx
	;mov	[ebp + 16], esi

	;mov	edi, _sbss
	;mov	ecx, _ebss
	;sub	ecx, edi
	;;sub	edi, _scode
	;add edi, phys - base
	;xor	eax, eax
	;rep stosd

	push	ebx
	push	dword mboot + phys - base
	call	_ArchStartup
	jmp	$

	align 4

[section .data]
_kernel_startup	times 7 dd 0
