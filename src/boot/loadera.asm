; I tried dis-assembling the INT 15h AH=89h function in my BIOS.
; It doesn't appear to support 32-bit pmode.

SEGMENT _TEXT PUBLIC CLASS=CODE

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			_intr
; action:		replaces buggy Turbo C 2.0 intr() function
; in:			interrupt number and pointer to REGPACK on stack
; out:			(nothing)
; modifies:		REGPACK
; minimum CPU:		8088
; notes:		C proto: void intr(int intnum, struct REGPACK *preg);
;			Turbo C 2.0 intr() does not appear to load BP
;			register correctly
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%if 1

GLOBAL _intr
_intr:
	push bp
		mov bp,sp

; save old registers
		pushf
		push es
		push ds
		push di
		push si
		push bp
		push dx
		push cx
		push bx
		push ax

; set interrupt vector, using self-modifying code
			mov bx,word [bp + 4]
			mov byte [cs:INT_NUM],bl

; load new registers
			mov bx,word [bp + 6]
			push bx
			push ds

			mov ax,word [bx + 0]	; [preg + 0]
			push word [bx + 2]	; [preg + 2]; BX
			mov cx,word [bx + 4]	; [preg + 4]
			mov dx,word [bx + 6]	; [preg + 6]
			mov bp,word [bx + 8]	; [preg + 8]
			mov si,word [bx + 10]	; [preg + 10]
			mov di,word [bx + 12]	; [preg + 12]
			push word [bx + 14]	; [preg + 14]; DS
			mov es,word [bx + 16]	; [preg + 16]
			push word [bx + 18]	; [preg + 18]; FLAGS

			popf
			pop ds
			pop bx

; perform interrupt (CDh = opcode for INT instruction)
			db 0CDh
INT_NUM:
			db 10h

; save new registers
			push bx
			push ds
			pushf

			mov bx,sp
			mov ds,[ss:bx + 6]
			mov bx,[ss:bx + 8]

			pop word [bx + 18]	; [preg + 18]; FLAGS
			mov word [bx + 16],es	; [preg + 16]
			pop word [bx + 14]	; [preg + 14]; DS
			mov word [bx + 12],di	; [preg + 12]
			mov word [bx + 10],si	; [preg + 10]
			mov word [bx + 8],bp	; [preg + 8]
			mov word [bx + 6],dx	; [preg + 6]
			mov word [bx + 4],cx	; [preg + 4]
			pop word [bx + 2]	; [preg + 2]; BX
			mov word [bx + 0],ax	; [preg + 0]

			add sp,4

; restore old registers
		pop ax
		pop bx
		pop cx
		pop dx
		pop bp
		pop si
		pop di
		pop ds
		pop es
		popf
	pop bp
	ret

%endif

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

enable_a20:
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

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			_enable_pmode
; action:		enables 32-bit pmode and jumps to loaded kernel
; in:			A20 flag and 32-bit physical entry point on stack
; out (error):		AX=1 if 32-bit CPU not present
; out (error):		AX=2 if A20 could not be enabled
; out (success):	(does not return if successful)
; modifies:		AX
; minimum CPU:		386+
; notes:		C prototype:
;			extern int enable_pmode(int enable_a20, long entry);
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

GLOBAL _enable_pmode
_enable_pmode:
	push bp
		mov bp,sp

; paranoia
EXTERN __got_32bit_cpu
		mov al,[__got_32bit_cpu]
		or al,al
		mov ax,1
		je pmode_err

; turn on A20 gate if necessary
		test word [bp + 4],0FFFFh
		je pmode_a20
		call enable_a20
		or ax,ax
		mov ax,2
		jne pmode_err
pmode_a20:
; stash the entry point. This is self-modifying code,
; but it's the only way I can see of jumping to the kernel
; with all of the data segment registers already loaded
		mov eax,[bp + 6]
		mov [cs:entry + 2],eax

; in gdt_ptr: convert GDT offset to GDT linear address
		xor eax,eax
		mov ax,ds
		shl eax,4
		add [gdt_ptr + 2],eax

; interrupts off
		push dword 0
		popfd

; convert SS:SP to ESP
		xor ebx,ebx
		mov bx,sp
		add ebx,eax
		mov esp,ebx

; load GDT and set PE bit
		lgdt [gdt_ptr]
		mov ebx,cr0
		inc bx
		mov cr0,ebx

; load data segment registers
		mov ax,DATA_SEL
		mov ds,ax
		mov ss,ax
		mov es,ax
		mov fs,ax
		mov gs,ax

; BING magic value. GRUB/Multiboot uses 2BADB002h
		mov eax,0B1C6B002h

; 2-byte JMP at [entry+0]:	66 EA
; 4-byte offset at [entry+2]:	00 00 00 00
; 2-byte selector at [entry+6]:	08 00	(=CODE_SEL)
entry:
		jmp CODE_SEL:dword 0
pmode_err:
	pop bp
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; data
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

SEGMENT _DATA PUBLIC CLASS=DATA

gdt:
; our NULL (0th) descriptor; INT 15h AH=87h 1st descriptor
	dw 0			; limit 15:0
	dw 0			; base 15:0
	db 0			; base 23:16
	db 0			; type
	db 0			; limit 19:16, flags
	db 0			; base 31:24

	dw 0
	dw 0
	db 0
	db 0
	db 0
	db 0

; src and dst descriptors used by INT 15h AH=87h
gdt2:
	dw 0FFFFh
	dw 0
	db 0
	db 93h
	db 0CFh
	db 0
gdt3:
	dw 0FFFFh
	dw 0
	db 0
	db 93h
	db 0CFh
	db 0

; two more descriptors used by INT 15h AH=87h
	dw 0
	dw 0
	db 0
	db 0
	db 0
	db 0

	dw 0
	dw 0
	db 0
	db 0
	db 0
	db 0

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
	dw gdt, 0		; old TLINK chokes; use 16-bit address here


%else

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; name:			_enable_pmode
; action:		enables 32-bit pmode and jumps to loaded kernel
; in:			A20 flag and 32-bit physical entry point on stack
; out (error):		AX=1 if 32-bit CPU not present
; out (error):		AX=2 if A20 could not be enabled
; out (success):	(does not return if successful)
; modifies:		AX
; minimum CPU:		386+
; notes:		C prototype:
;			extern int enable_pmode(int enable_a20, long entry);
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

GLOBAL _enable_pmode
_enable_pmode:
	push bp
		mov bp,sp

; paranoia
EXTERN __got_32bit_cpu
		mov al,[__got_32bit_cpu]
		or al,al
		mov ax,1
		je pmode_err

; stash the entry point. This is self-modifying code,
; but it's the only way I can see of jumping to the kernel
; with all of the data segment registers already loaded
		mov eax,[bp + 6]
		mov [cs:entry + 1],eax

; in gdt_ptr: convert GDT offset to GDT linear address
		xor eax,eax
		mov ax,ds
		shl eax,4
		add [gdt1 + 2],eax

; convert SS:SP to ESP
		xor ebx,ebx
		mov bx,sp
		add ebx,eax
		mov esp,ebx

; call INT 15h AH=89h to switch to pmode
		mov ah,89h
		mov bx,2820h	; move IRQs 0-15 to INTs 20h-2Fh
		push ds
		pop es
		mov di,gdt
		int 15h
		jc pmode_err

[BITS 32]

; BING magic value. GRUB/Multiboot uses 2BADB002h
		mov eax,0B1C6B002h

; 1-byte JMP at [entry+0]:	EA
; 4-byte offset at [entry+1]:	00 00 00 00
; 2-byte selector at [entry+5]:	08 00	(CODE_SEL)

entry:
		jmp CODE_SEL:0
[BITS 16]

pmode_err:
	pop bp
	ret

SEGMENT _DATA PUBLIC CLASS=DATA

gdt:
; NULL descriptor
	dw 0			; limit 15:0
	dw 0			; base 15:0
	db 0			; base 23:16
	db 0			; type
	db 0			; limit 19:16, flags
	db 0			; base 31:24

gdt1:
; pseudo-descriptor for the GDT itself
	dw gdt_end - gdt	; limit 15:0
; xxx - TLINK chokes on 32-bit address
;	dd gdt			; base 31:0
	dw gdt			; base 15:0
	dw 0			; base 31:16
	dw 0

; IDT pseudo-descriptor
	dw 0
	dd 0
	dw 0

; data descriptor for DS
	dw 0FFFFh
	dw 0
	db 0
	db 92h			; present, ring 0, data, expand-up, writable
	db 0CFh			; page-granular, 32-bit
	db 0

; data descriptor for ES
	dw 0FFFFh
	dw 0
	db 0
	db 92h
	db 0CFh
	db 0

; data descriptor for SS
	dw 0FFFFh
	dw 0
	db 0
	db 92h
	db 0CFh
	db 0

; code descriptor for CS
CODE_SEL	equ	$-gdt
	dw 0FFFFh
	dw 0
	db 0
	db 9Ah			; present, ring 0, code, non-conforming, readable
	db 0CFh			; page-granular, 32-bit
	db 0

; used by INT 15h AH=89h
	dd 0
	dd 0
gdt_end:

%endif

; OK, I'm not 100% sure what's going on here... had to fiddle
; about until things started working. Looks like you need to
; 1. name the segments _TEXT and _DATA
; 2. assign them to classes CODE and DATA
; 3. use a GROUP statement: (NO, DON'T DO THIS)

GROUP DGROUP _TEXT _DATA
