[bits		16]
[org		100h]
[section	.text]

%include	"gdtnasm.inc"

%define		ONE_MEG		0x100000
%define		MEM_INCR	ONE_MEG
%define		LOAD_ADDR	ONE_MEG
%define		RDSK_ADDR	(ONE_MEG * 2)
%define		BUFFER_ADDR	0xfe00 ;0x20000

%define		FS_OK		1
%define		FS_ERROR	0

start:
	mov		si, msg_loader
	call	print

	mov		[boot_drv], dl


	;******************************************************
	; Switch to "flat-real" mode
	;******************************************************

	cli
n5:	in		al, 0x64		; Enable A20 gate
	test	al, 2
	jnz		n5
	mov		al, 0xD1
	out		0x64, al
n6:	in		al, 0x64
	test	al, 2
	jnz		n6
	mov		al, 0xDF
	out		0x60, al

	xor eax, eax
	mov ax, cs				; get code segment
	shl eax, 4				; convert segment -> linear
	add [gdtinfo+2], eax	; do the fixups
	add [loader_code+2], eax
	add [loader_data+2], eax
	
	lgdt	[gdtinfo]		; Load GDTR
	mov 	ecx, CR0		; Set CR0.PE
	inc		cx
	mov		CR0, ecx

	mov		ax, flat_data - gdt	; Selector for 4Gb data seg
	mov		ds, ax			; Extend limit for ds
	mov		es, ax			; Extend limit for es
	mov		fs, ax			; fs and...
	mov 	gs, ax			; gs
	dec		cx				; switch back to real mode
	mov		CR0, ecx
	sti

	mov		ax, cs
	mov		ds, ax


	;******************************************************
	; Count installed memory
	;******************************************************
	
	mov		si, msg_memory
	call	print

count_memory:
	; set, restore and test a byte at each 1 Mb boundary
	; the end is reached when byte read != byte written
	xor     edi, edi
.start:
	mov     al, byte [es:edi]		; Save the byte
	mov     byte [es:edi], 0aah		; Write a new one (0AAh)
	mov     ah, byte [es:edi]		; Read it back
	mov     byte [es:edi],al		; Restore the old one
	cmp     ah, 0aah				; Check the byte...
	jne     .finish					; ...if it's different, stop

	mov     al, byte [es:edi]		; Repeat for 55h
	mov     byte [es:edi], 55h
	mov     ah, byte [es:edi]
	mov     byte [es:edi],al
	cmp     ah, 55h
	jne     .finish

	add     edi,MEM_INCR			; Skip to the next chunk
	jmp     .start

.finish:
	mov     [memory_top],edi		; edi contains failed address

	mov		eax, edi				; Remind the user how much memory they have
	shr		eax, 20
	call	print_int
	mov		si, msg_memory.mb
	call	print
	jmp		load_kernel

load_fail:							; Generic failure proc used during loading
	mov		si, msg_failed
	call	print
	jmp		$

	;******************************************************
	; Load the kernel into extended memory
	;******************************************************

load_kernel:
	mov		si, msg_fs_init
	call	print
	
	call	fs_init					; Initialize the file system

	test	al, al
	jz		load_fail
	mov		si, msg_done
	call	print

	mov		si, msg_loading_kernel
	call	print

	mov		ebx, LOAD_ADDR			; Load to LOAD_ADDR
	mov		si, kernel_name
	
	call	fs_loadfile				; Load the kernel

	test	al, al
	jz		load_fail

	xor		eax, eax				; Get the size of the kernel in sectors
	mov		ax, [file_scts]
	shl		eax, 9
	mov		[kernel_size], eax		; Store the size of the kernel
	;call	print_int
	mov		si, msg_done
	call	print

;%if 0
	mov	si, msg_loading_ramdisk
	call	print

	mov	ebx, RDSK_ADDR			; Load to RDSK_ADDR
	mov	si, ramdisk_name
		
load_file:
	call	fs_loadfile				; Load the ramdisk

	test	al, al
	jz	load_fail

	xor	eax, eax				; Get the size of the ramdisk in sectors
	mov	ax, [file_scts]
	shl	eax, 9
	add	[rdsk_size], eax		; Store the size of the ramdisk
	;call	print_int
	add	ebx, eax
	add	si, 12

	mov	si, msg_done
	call	print

;%endif

	mov		dx, 0x3f2				; Turn off the floppy motor
	mov		al, 0x0c
	out		dx, al
	
	;******************************************************
	; Enable full protected mode
	;******************************************************

	; Interrupts make this thing go *bang* from now on

	cli

	mov		ecx, cr0
	inc		cx						; Set CR0.PE for the last time...
	mov		cr0, ecx
	mov		ax, loader_data - gdt
	mov		fs, ax					; fs = loader data
	mov		ax, flat_data - gdt
	mov		gs, ax					; gs = linear data
	mov		ax, kernel_data - gdt
	mov		ds, ax					; ds = es = ss = kernel data
	mov		es, ax
	mov		ss, ax

	; Reload CS to complete the move to protected mode
	jmp		dword (loader_code-gdt):pmode1

%include "minifs.asm"				; File system procedures (currently FAT12)

;**********************************************************
;* print: print a string using the BIOS
;*
;* in:
;*	 si: address of message (NUL-terminated)
;* modifies:
;*	nothing
;**********************************************************

print:
	push	ax						; Save some registers
	push	bx
	; INT 10/0Eh has been known to use BP, but we don't need it

	mov		ah, 0eh
	xor		bx, bx					; Page 0

.1:
	mov		al, [si]				; Load a byte...
	test	al, al					; ...stop on NUL
	jz		.2
	int		10h						; Write it
	inc		si
	jmp		.1

.2:
	pop		bx
	pop		ax
	ret

;**********************************************************
;* print_int: print a 32-bit number in decimal using the BIOS
;*
;* in:
;*	eax: number to be printed
;* modifies:
;*	nothing
;**********************************************************

print_int:
	push ebx				; save registers
	push ecx
	push edx
	push esi
	push edi


	mov esi, 0				; don't print leading zeroes
	mov ebx, 1000000000
	mov ecx, eax			; store a copy of ax


.pipm:
	cmp ebx, 1				; if we are on last digit
	jne .cnpm
	mov esi, 1				; make sure we print it (even if zero)
.cnpm:
	mov eax, ecx			; get current num
	xor edx, edx			; clear dx
	div ebx					; divide by decimal
	mov ecx, edx			; save remainder
	or	esi, eax			; if si and ax are zero, it's a leading zero
	jz	.lz					; leading zero

	push ebx
	xor bx, bx
	add al, '0'
	mov ah, 0eh
	int 10h
	pop ebx

.lz:
	mov eax, ebx			; we have to reduce the radix too
	xor edx, edx			; clear dx
	mov ebx, 10				; use 10 as new divider
	div ebx					; divide
	mov ebx, eax			; save result
	or	ebx, ebx			; if zero, we're done
	jnz .pipm

	pop edi					; restore registers
	pop esi
	pop edx
	pop ecx
	pop ebx

	ret



[bits 32]

	;******************************************************
	; Protected-mode code
	;******************************************************


pmode1:
	push	dword 2				; clear EFLAGS apart from bit 1
	popfd

%if 0
	mov		al, byte [gs:RDSK_ADDR + 0x1000]
	mov		byte [gs:0b8000h], al
	mov		al, byte [gs:RDSK_ADDR + 0x1001]
	mov		byte [gs:0b8002h], al
	mov		al, byte [gs:RDSK_ADDR + 0x1002]
	mov		byte [gs:0b8004h], al
	mov		al, byte [gs:RDSK_ADDR + 0x1003]
	mov		byte [gs:0b8006h], al
%endif

	; Set up the kernel's entry variables
	mov		eax, dword [fs:memory_top]
	mov		ebx, dword [fs:kernel_size]
	mov		ecx, LOAD_ADDR
	mov		edx, dword [fs:rdsk_size]
	mov		esi, RDSK_ADDR

	; Start the kernel!
	jmp		(kernel_code-gdt):0x1000
	;jmp $


[section	.data]
gdtinfo:	dw	gdtlength
			dd	gdt

gdt:

null_desc	desc	0,0,0
loader_code	desc	0, 0xFFFF, D_CODE + D_READ + D_BIG + D_BIG_LIM
loader_data	desc	0, 0xFFFF, D_DATA + D_WRITE + D_BIG + D_BIG_LIM
kernel_code	desc	LOAD_ADDR, 0xFFFFF, D_CODE + D_READ + D_BIG + D_BIG_LIM
kernel_data	desc	LOAD_ADDR, 0xFFFFF, D_DATA + D_WRITE + D_BIG + D_BIG_LIM
flat_code	desc	0, 0xFFFFF, D_CODE + D_READ + D_BIG + D_BIG_LIM
flat_data	desc	0, 0xFFFFF, D_DATA + D_WRITE + D_BIG + D_BIG_LIM

gdtlength	equ $ - gdt - 1

msg_loader:	db		13, 10, "Starting The M”bius...", 13, 10, 0
msg_memory:	db		"System memory: ", 0
.mb:		db		"MB", 13, 10, 0
msg_fs_init:
			db		"Starting file system", 0
msg_done:	db		" OK", 13, 10, 0
msg_failed:	db		" failed", 13, 10, 0
msg_loading_kernel:
			db		"Loading the kernel", 13, 10, 0
msg_loading_ramdisk:
			db		"Loading the ramdisk", 13, 10, 0

kernel_name:
			db		"KERNEL  EXE", 0
;ramdisk_name:
;			db		"RAMDISK BIN", 0

boot_drv:	db	0
memory_top:	dd	0
kernel_size:dd	0
rdsk_size:	dd	0

sectorbuf:	times 512 resb 0
sectorbuf_end:

files:
		db	"ISA     CFG", 0
		db	"DRIVERS CFG", 0
		db	"PCI     DRV", 0
		db	"ISA     DRV", 0
		db	"PS2MOUSEDRV", 0
		db	"SERMOUSEDRV", 0
		db	"KEYBOARDDRV", 0
		db	"ATA     DRV", 0
		db	"FDC     DRV", 0
		db	"FAT     DRV", 0
		db	"VGATEXT DRV", 0
		db	"CMOS    DRV", 0
		db	"KDEBUG  DLL", 0
		db	"DEVTEST EXE", 0
		db	"KERNELU DLL", 0
		db	"LIBC    DLL", 0
		db	"TETRIS  EXE", 0
		db	"SHORT   EXE", 0
		db	"CONSOLE EXE", 0
		db	"HELLO   EXE", 0
files_end:
