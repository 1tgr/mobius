; $Id: loaderb.asm,v 1.3 2002/02/22 15:27:06 pavlovskii Exp $
; I tried dis-assembling the INT 15h AH=89h function in my BIOS.
; It doesn't appear to support 32-bit pmode.

SEGMENT _TEXT PUBLIC CLASS=CODE

%include "gdtnasm.inc"
%define		ONE_MEG		0x100000
%define		LOAD_ADDR	ONE_MEG
%define		RDSK_ADDR	(ONE_MEG * 2)

%if 1

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			verify_a20
; action:		verifies A20 gate is enabled (ON)
; in:			(nothing)
; out (A20 enabled):	ZF=0
; out (A20 NOT enabled):ZF=1
; modifies:		(nothing)
; minimum CPU:		286+
; notes:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

verify_a20:
	push ax
	push ds
	push es
		xor ax,ax
		mov ds,ax
		dec ax
		mov es,ax

		mov ax,[es:10h]		; read word at FFFF:0010 (1 meg)
		not ax			; 1's complement
		push word [0]		; save word at 0000:0000 (0)
			mov [0],ax	; word at 0 = ~(word at 1 meg)
			mov ax,[0]	; read it back
			cmp ax,[es:10h]	; fail if word at 0 == word at 1 meg
		pop word [0]
	pop es
	pop ds
	pop ax
	ret		; if ZF=1, the A20 gate is NOT enabled

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			empty_8042
; action:		waits until 8042 keyboard controller ready
;			to accept a command or data byte
; in:			(nothing)
; out:			(nothing)
; modifies:		AL
; minimum CPU:		286+
; notes:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

empty:
	jmp short $+2	; a delay (probably not effective nor necessary)
	in al,60h	; read and discard data/status from 8042
empty_8042:
	jmp short $+2	; delay
	in al,64h
	test al,1	; output buffer (data _from_ keyboard) full?
	jnz empty	; yes, read and discard
	test al,2	; input buffer (data _to_ keyboard) empty?
	jnz empty_8042	; no, loop
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			enable_a20_at
; action:		enables A20 by setting b1 of 8042 output port
; in:			(nothing)
; out:			(nothing)
; modifies:		AX
; minimum CPU:		286+
; notes:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

enable_a20_at:
	pushf
		cli
		call empty_8042
		mov al,0D0h	; 8042 command byte to read output port
		out 64h,al
await:
		in al,64h
		test al,1	; output buffer (data _from_ keyboard) full?
		jz await	; no, loop

		in al,60h	; read output port
		or al,2		; OR with 2 to enable gate
		mov ah,al

		call empty_8042
		mov al,0D1h	; 8042 command byte to write output port
		out 64h,al

		call empty_8042
		mov al,ah	; the value to write
		out 60h,al

		call empty_8042
	popf
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			enable_a20_vectra
; action:		enables A20 with 8042 command byte DFh
; in:			(nothing)
; out:			(nothing)
; modifies:		AX
; minimum CPU:		286+
; notes:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

enable_a20_vectra:
	pushf
		cli
		call empty_8042
		mov al,0DFh
		out 64h,al
		call empty_8042
	popf
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			enable_a20_ps2
; action:		enables A20 gate using INT 15h AX=2401h
; in:			(nothing)
; out:			(nothing)
; modifies:		(nothing)
; minimum CPU:		286+
; notes:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

enable_a20_ps2:
	push ax
		mov ax,2401h
		int 15h
	pop ax
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			enable_a20
; action:		enables A20 gate and verifies that it's on
; in:			(nothing)
; out (success):	AX=0
; out (failure):	AX=1
; modifies:		AX
; minimum CPU:		286+
; notes:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

GLOBAL _enable_a20
_enable_a20:
	call enable_a20_at	; try 'AT' method
	call verify_a20		; did it work?
	jne enable_a20_1
	call enable_a20_vectra	; no; try 'Vectra' method
	call verify_a20		; did _that_ work?
	jne enable_a20_1
	call enable_a20_ps2	; no; try 'PS/2' method
	call verify_a20		; last chance: did it work?
enable_a20_1:
	mov ax,0
	jne enable_a20_2
	mov ax,1
enable_a20_2:
	ret

GLOBAL _enable_frm
_enable_frm:
	push bp
	mov bp,sp

	; paranoia
EXTERN __got_32bit_cpu
	mov al,[__got_32bit_cpu]
	or al,al
	mov ax,1
	je .pmode_err

	cli

; turn on A20 gate if necessary
	test word [bp + 4],0FFFFh
	je .pmode_a20
	call _enable_a20
	or ax,ax
	mov ax,2
	jne .pmode_err

.pmode_a20:
	xor	eax, eax
	mov	ax, ds			; get code segment
	shl	eax, 4			; convert segment -> linear
	add	[gdtinfo+2], eax	; do the fixups
	
	mov	[loader_code+2], ax
	mov	[loader_data+2], ax
	shr	eax, 16
	mov	[loader_code+4], al
	mov	[loader_data+4], al
	mov	[loader_code+7], ah
	mov	[loader_data+7], ah
	
	lgdt	[gdtinfo]		; Load GDTR
		
	mov	ecx, CR0		; Set CR0.PE
	inc	cx
	mov	CR0, ecx

	jmp	short .flush
.flush:

	mov	ax, flat_data - gdt	; Selector for 4Gb data seg
	mov	ds, ax			; Extend limit for ds
	
	mov	es, ax			; Extend limit for es
	mov	fs, ax			; fs and...
	mov	gs, ax			; gs
	dec	cx			; switch back to real mode
	mov	CR0, ecx
	sti

	mov	ax, cs
	mov	ds, ax

	xor ax, ax
	mov sp,bp
	pop bp
	ret

.pmode_err:
	sti
	mov sp,bp
	pop bp
	ret

GLOBAL	_copy_to_extended
_copy_to_extended:
	push	bp
	mov	bp, sp

	mov	edi, dword [bp + 4]
	xor	ax, ax
	mov	es, ax

	xor	esi, esi
	mov	si, word [bp + 8]
	;mov	ax, ds
	;shl	ax, 4
	;add	si, ax

	xor	ecx, ecx
	mov	cx, word [bp + 10]

	a32 rep movsb

	mov	sp, bp
	pop	bp
	ret

GLOBAL	_start_kernel
_start_kernel:
	push	bp
	mov	bp, sp

	;mov	ax, 0092h
	;int	10h

	; Patch the loader base address
	mov	ecx, cs
	shl	ecx, 4
	mov	[loader_code+2], cx
	mov	[loader_data+2], cx
	shr	ecx, 16
	mov	[loader_code+4], cl
	mov	[loader_data+4], cl
	mov	[loader_code+7], ch
	mov	[loader_data+7], ch

	; Set the kernel base address
	mov	ecx, [bp + 4]
	mov	[kernel_code+2], cx
	mov	[kernel_data+2], cx
	shr	ecx, 16
	mov	[kernel_code+4], cl
	mov	[kernel_data+4], cl
	mov	[kernel_code+7], ch
	mov	[kernel_data+7], ch

	mov	dx, 0x3f2				; Turn off the floppy motor
	mov	al, 0x0c
	out	dx, al
	
	;******************************************************
	; Enable full protected mode
	;******************************************************

	; kernel address
	mov	ecx, [bp + 4]
	; ramdisk address
	mov	esi, [bp + 8]
	
	; Interrupts make this thing go *bang* from now on

	cli
	
	mov	eax, cr0
	inc	ax					; Set CR0.PE for the last time...
	mov	cr0, eax

	mov	ax, loader_data - gdt
	mov	fs, ax					; fs = loader data
	mov	ax, flat_data - gdt
	mov	gs, ax					; gs = linear data
	mov	ax, kernel_data - gdt
	mov	ds, ax					; ds = es = ss = kernel data
	mov	es, ax
	mov	ss, ax

	; Reload CS to complete the move to protected mode
	jmp	dword (loader_code-gdt):pmode1

	; Not much point in this bit, is there? :)
	mov	sp, bp
	pop	bp
	ret

[bits 32]
pmode1:
	;push	dword 2				; clear EFLAGS apart from bit 1
	;popfd

%if 1
	mov		al, byte [gs:esi+0x1000]
	mov		byte [gs:0b8000h], al
	mov		al, byte [gs:esi+0x1001]
	mov		byte [gs:0b8002h], al
	mov		al, byte [gs:esi+0x1002]
	mov		byte [gs:0b8004h], al
	mov		al, byte [gs:esi+0x1003]
	mov		byte [gs:0b8006h], al
%endif

	; Set up the kernel's entry variables

EXTERN _memory_top
EXTERN _kernel_size
EXTERN _rdsk_size

	;            eax -> address at top of memory
	;            ebx -> size of kernel
	;            ecx -> physical address of start
	;            edx -> size of ramdisk
	;            esi -> physical address of ramdisk
	; cs, ds, es, ss -> base = start
	;             fs -> base = start of loader (.COM program)
	;             gs -> linear (base = 0)

	mov		eax, dword [fs:_memory_top]
	mov		ebx, dword [fs:_kernel_size]
	mov		edx, dword [fs:_rdsk_size]
	
	;jmp		$
	; Start the kernel!
	jmp		(kernel_code-gdt):0x1000

	;jmp	$

%endif

gdtinfo:	dw	gdtlength
		dd	gdt

gdt:

null_desc	desc	0,0,0
loader_code	desc	0, 0xFFFF, D_CODE + D_READ + D_BIG + D_BIG_LIM
loader_data	desc	0, 0xFFFF, D_DATA + D_WRITE + D_BIG + D_BIG_LIM
kernel_code	desc	0xdeadbeef, 0xFFFFF, D_CODE + D_READ + D_BIG + D_BIG_LIM
kernel_data	desc	0xdeadbeef, 0xFFFFF, D_DATA + D_WRITE + D_BIG + D_BIG_LIM
flat_code	desc	0, 0xFFFFF, D_CODE + D_READ + D_BIG + D_BIG_LIM
flat_data	desc	0, 0xFFFFF, D_DATA + D_WRITE + D_BIG + D_BIG_LIM

gdtlength	equ $ - gdt - 1

kernel:		dd	0
ramdisk:	dd	0

; OK, I'm not 100% sure what's going on here... had to fiddle
; about until things started working. Looks like you need to
; 1. name the segments _TEXT and _DATA
; 2. assign them to classes CODE and DATA
; 3. use a GROUP statement: (NO, DON'T DO THIS)

SEGMENT _DATA PUBLIC CLASS=DATA

GROUP DGROUP _TEXT _DATA
