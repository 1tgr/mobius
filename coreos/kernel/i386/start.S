/* boot.S - bootstrap the kernel */
/* Copyright (C) 1999, 2001  Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define ASM	1
#define HAVE_ASM_USCORE
#include <kernel/multiboot.h>
#include <kernel/addresses.h>
#define PAGE_TABENT(X)	((((X) >> 12) & 0x3FF) << 2)
#define PAGE_DIRENT(X)	((((X) >> 22) & 0x3FF) << 2)
#define V2P(x)		((x) - ADDR_KERNEL_VIRT + ADDR_KERNEL_PHYS)
#define P2V(x)		((x) + ADDR_KERNEL_VIRT - ADDR_KERNEL_PHYS)

	.text

	/* Align 32 bits boundary.  */
	.align	4
	
	/* Multiboot header.  */
	.globl	_multiboot_header
_multiboot_header:
	/* magic */
	.long	MULTIBOOT_HEADER_MAGIC
	/* flags */
	.long	MULTIBOOT_HEADER_FLAGS
	/* checksum */
	.long	-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
#ifndef __ELF__
	/* header_addr */
	.long	V2P(_multiboot_header)
	/* load_addr */
	.long	ADDR_KERNEL_PHYS
	/* load_end_addr */
	.long	V2P(__end__)
	/* bss_end_addr */
	.long	V2P(__bss_end__)
	/* entry_addr */
	.long	V2P(_KernelEntry)
#endif /* ! __ELF__ */

.globl _KernelEntry
_KernelEntry:
	/* Initialize the stack pointer.  */
	movl	$(V2P(_start_stack + STACK_SIZE)), %esp

	mov		$(V2P(_sbss)), %edi
	xor		%eax, %eax
	mov		$(V2P(_ebss)), %ecx
	sub		%edi, %ecx
	shr		$2, %ecx
	rep
	stosl
	
	/* Reset EFLAGS.  */
	pushl	$0
	popf

	/* Push the pointer to the Multiboot information structure.  */
	pushl	%ebx

	/*
	 * Map bottom 8MB virtual to bottom 8MB physical
	 */
	movl	$0, V2P(_identity_pagetable)
	movl	$0x1007, %eax
	movl	$(V2P(_identity_pagetable) + 4), %edi
	movl	$2047, %ecx
map_memory_loop:
	movl	%eax, (%edi)
	addl	$0x1000, %eax
	addl	$4, %edi
	loop	map_memory_loop

	/*
	 * Map kernel. This code assumes that the kernel doesn't span a 4MB 
	 *	virtual boundary.
	 */
	movl	$(ADDR_KERNEL_PHYS + 7), %eax
	movl	$(V2P(_kernel_pagetable)), %edi
	movl	$(__bss_end__ - ADDR_KERNEL_VIRT), %ecx
	shrl	$12, %ecx
	incl	%ecx
map_kernel_loop:
	movl	%eax, (%edi)
	addl	$0x1000, %eax
	addl	$4, %edi
	loop	map_kernel_loop

	/* Make the page directory the top page table */
	movl	$(V2P(_kernel_pagedir) + 7), V2P(_kernel_pagedir + 1023*4)

	movl	$(V2P(_identity_pagetable) + 7), V2P(_kernel_pagedir)
	movl	$(V2P(_kernel_pagetable) + 7), V2P(_kernel_pagedir + PAGE_DIRENT(ADDR_KERNEL_VIRT))
	movl	$(V2P(_identity_pagetable) + 7), V2P(_kernel_pagedir + PAGE_DIRENT(ADDR_TEMP_VIRT))
	movl	$(V2P(_identity2_pagetable) + 7), V2P(_kernel_pagedir + PAGE_DIRENT(ADDR_TEMP_VIRT) + 4)

	movl	$(V2P(_kernel_pagedir)), %eax
	movl	%eax, %cr3
	movl	%cr0, %eax
	orl		$0x80000000, %eax
	movl	%eax, %cr0

	mov	$paging, %eax
	jmp	*%eax
paging:
	movl	$0, _kernel_pagedir
	add		$(ADDR_KERNEL_VIRT - ADDR_KERNEL_PHYS), %esp

	/* Restore Multiboot info */
	pop		%ebx

	add		$ADDR_TEMP_VIRT, %ebx
	push	%ebx
	push	_multiboot_header

	/* Now enter the C main function...  */
	call	EXT_C(ArchStartup)

loop:	hlt
	jmp	loop

	.section .pages, "bw"
	.globl _kernel_pagedir
_kernel_pagedir:		.fill 4096
_identity_pagetable:	.fill 4096
_identity2_pagetable:	.fill 4096
_kernel_pagetable:		.fill 4096

	.bss
	/* Our stack area.  */
.globl _start_stack
_start_stack:				.fill STACK_SIZE
