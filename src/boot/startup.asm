; $Id: startup.asm,v 1.3 2002/02/22 15:31:19 pavlovskii Exp $

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; stripped-down startup code
;
; NO HEAP...NO #pragma startup/#pragma exit/atexit()...
; NO EXIT CODE...TINY MEMORY MODEL ONLY...NO NULL POINTER DETECTION...
;
; Thanks, John Fine, for helping with the invalid entry point problem.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

SEGMENT _TEXT PUBLIC CLASS=CODE

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; startup code
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	resb 100h

GLOBAL begin
begin:

..start:
	mov ax,cs			; zero the BSS
	mov ss,ax
	mov sp,0xffff
	xor ax,ax
	mov di,bdata
	mov cx,edata
	sub cx,di
	rep stosb

EXTERN _boot_dev
	mov [_boot_dev], dl

	call cpu_is_32bit
EXTERN __got_32bit_cpu
	mov [__got_32bit_cpu],ah

	call get_extmem_size
EXTERN __ext_mem_size
	mov [__ext_mem_size + 0],ax
	mov [__ext_mem_size + 2],dx

	call get_convmem_size
EXTERN __conv_mem_size
	mov [__conv_mem_size + 0],ax
	mov [__conv_mem_size + 2],dx

EXTERN _main
	call _main			; call C code

exit:
%ifdef NODOS
	int 19h				; re-start the boot process
%else
	mov ax,4C01h
	int 21h				; exit to DOS with errorlevel 1
%endif

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			cpu_is_32bit
; action:		checks for 32-bit CPU
; in:			(nothing)
; out:			AX != 0 (AH != 0, actually) if 32-bit CPU
; modifies:		AX
; minimum CPU:		8088
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

cpu_is_32bit:
	push bx
		pushf
			pushf
			pop bx		; old FLAGS -> BX
			mov ax,bx
			xor ah,70h	; try changing b14 (NT)...
			push ax		; ... or b13:b12 (IOPL)
			popf
			pushf
			pop ax		; new FLAGS -> AX
		popf
		xor al,al		; zero AL
		xor ah,bh		; 32-bit CPU if we changed NT...
		and ah,70h		; ...or IOPL
	pop bx
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			get_convmem_size
; action:		gets conventional memory size using BIOS calls
; in:			(nothing)
; out:			32-bit conventional memory size in DX:AX
; modifies:		DX, AX
; minimum CPU:		8088
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

get_convmem_size:
	int 12h

	xor dx,dx		; move to DX multiplies by 256
	mov dl,ah
	mov ah,al
	xor al,al

	shl ax,1		; multiply by 4 (x1024 total)
	rcl dx,1
	shl ax,1
	rcl dx,1
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			get_extmem_size
; action:		gets extended memory size using BIOS calls
; in:			(nothing)
; out:			32-bit extended memory size in DX:AX
; modifies:		DX, AX
; minimum CPU:		8088 (386+ for INT 15h AX=E820h)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

get_extmem_size:
	push es
	push di
	push cx
	push bx

EXTERN __got_32bit_cpu
		mov al,[__got_32bit_cpu]
		or al,al
		je mem3			; can't use INT 15h AX=E820h

; try INT 15h AX=E820h
		push ds
		pop es
		mov di,buffer_e820
		xor esi,esi
		mov edx,534D4150h	; "SMAP"
		xor ebx,ebx
mem1:
		mov ecx,20h
		mov eax,0000E820h
		int 15h
		jc mem3
		cmp dword [es:di + 16],1 ; type 1 memory (available to OS)
		jne mem2
		cmp dword [es:di],100000h ; check only extended memory
		jb mem2
		add esi,[es:di + 8]
mem2:
		or ebx,ebx
		jne mem1

		mov eax,esi
		mov edx,esi
		shr edx,16
		jmp short mem7		; DX:AX=RAM top
mem3:
; try INT 15h AX=E801h
		mov ax,0E801h
		int 15h
		jc mem4
		mov dx,bx		; BX=ext mem >=16M, in 64K
					; move to DX multiples by 64K
		xor bx,bx
		mov bl,ah		; AX=ext mem 1M-16M, in K
		mov ah,al		; multiply by 256
		xor al,al

		shl ax,1		; multiply by 4 (x1024 total)
		rcl bx,1
		shl ax,1
		rcl bx,1

		add dx,bx
		jmp short mem7		; DX:AX=RAM top
mem4:
; try INT 15h AH=88h
		mov ax,8855h
		int 15h			; CY=error is not reliable...
		cmp ax,8855h		; ...so make sure AL is modified
		jne mem5
		mov ax,88AAh
		int 15h			; AX=ext mem size, in K
		cmp ax,88AAh
		je mem6
mem5:
		xor dx,dx		; move to DX multiplies by 256
		mov dl,ah
		mov ah,al
		xor al,al

		shl ax,1		; multiply by 4 (x1024 total)
		rcl dx,1
		shl ax,1
		rcl dx,1

		jmp short mem7		; DX:AX=RAM top
mem6:
		xor ax,ax
		xor dx,dx
mem7:
	pop bx
	pop cx
	pop di
	pop es
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			__REALCVT
; action:		traps attempts to use printf() with floating point
; in:			(nothing)
; out:			DOES NOT RETURN
; modifies:		AX
; minimum CPU:		8088 (only '286+ systems have A20 gate)
; notes:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

GLOBAL __REALCVT
__REALCVT:
	mov ax,0E07h			; *** BEEEP ***
	int 10h
	mov ah,0			; await key pressed
	int 16h
	jmp exit

SEGMENT _DATA PUBLIC CLASS=DATA

buffer_e820:
	times 42 db 0

gdt:
	dw 0			; limit 15:0
	dw 0			; base 15:0
	db 0			; base 23:16
	db 0			; type
	db 0			; limit 19:16, flags
	db 0			; base 31:24
CODE_SEL	equ	$-gdt
	dw 0FFFFh
	dw 0
	db 0
	db 9Ah			; present, ring 0, code, non-conforming, readable
	db 0CFh			; page-granular, 32-bit
	db 0
DATA_SEL	equ	$-gdt
	dw 0FFFFh
	dw 0
	db 0
	db 92h			; present, ring 0, data, expand-up, writable
	db 0CFh			; page-granular, 32-bit
	db 0
gdt_end:

gdt_ptr:
	dw gdt_end - gdt - 1	; GDT limit
;;	dd gdt			; linear, physical adr of GDT
	dw gdt, 0		; TLINK chokes; must use 16-bit address here

entry:
	dd 0			; far address of pmode kernel entry point
	dw CODE_SEL

SEGMENT _BSS PUBLIC CLASS=BSS
bdata:

SEGMENT _BSSEND PUBLIC CLASS=BSSEND
edata:

GROUP DGROUP _TEXT _DATA _BSS _BSSEND
