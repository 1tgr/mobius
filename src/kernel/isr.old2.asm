[bits		32]

[section	.text]
[extern		_isr]
[extern		_tss]
[extern		_current]

;[global		_old_current]
;_old_current:
	;dd		0

_isr_and_switch:
	push	gs						; push segment registers
	push	fs
	push	es
	push	ds
	pusha							; push GP registers
		
	mov		ax, 20h					; use the kernel selectors
	mov		ds, eax
	mov		es, eax
	mov		fs, eax
	mov		gs, eax

	mov		eax, [_current]			; save previous...
	;mov		[_old_current], eax		; ...thread pointer
	
	cmp		eax, 0
	je		.1
	mov		[eax + 4], esp			; if so, reload the stack

.1:
	call	_isr					; call the isr routine

	;mov		eax, [_old_current]		; did we switch threads?
	;cmp		eax, [_current]
	;je		.2

	;cmp		eax, 0
	;je		.4
	;mov		[eax + 4], esp			; if so, reload the stack
;.4:
	mov		ebx, [_current]			; modify the TSS with the new thread's ESP
	cmp		ebx, 0
	je		.3

	mov		esp, [ebx + 4]			; point it after the context record
	lea		ecx, [esp + 19*2]		; adjust for size of kernel context
	;cmp		dword [esp+32+16+24+8], 0x18
	;je		.updatetss
	add		ecx, 8*2
;.updatetss:
	mov		[_tss+4], ecx

	;cli
	;hlt

.2:
	mov		eax, [_current]			; reload CR3 if necessary
	mov		eax, [eax + 0]			; current->process

	mov		ebx, cr3
	mov		eax, [eax + 0]			; current->process->page_dir

	cmp		eax, ebx
	je		.3
	mov		cr3, eax

.3:
	popa							; 8 dwords (32 bytes)
	pop		ds						; 4 dwords (16 bytes)
	pop		es
	pop		fs
	pop		gs
	add		esp, 8					; 2 dwords (8 bytes; which_int and err_code)
	iret							; 5/3 dwords (20/12 bytes)
									;	Total:	17 dwords (switch from kernel mode)
									;			19 dwords (switch from user mode)

%macro		exception 1
[global		_exception_%1]

; error code already on stack

_exception_%1:
	push	dword 0x%1
	jmp 	_isr_and_switch
%endmacro

%macro		irq 1
[global		_irq_%1]

_irq_%1:
	push	dword -1	; `fake' error code
	push	dword 0x%1
	jmp 	_isr_and_switch
%endmacro

%macro		interrupt 1
[global		_interrupt_%1]

_interrupt_%1:
	push	dword 0
	push	dword 0x%1
	jmp 	_isr_and_switch
%endmacro

	interrupt		0
	interrupt		1
	interrupt		2
	interrupt		3
	interrupt		4
	interrupt		5
	interrupt		6
	interrupt		7
	exception		8
	interrupt		9
	exception		a
	exception		b
	exception		c
	exception		d
	exception		e
	interrupt		f
	interrupt		10
	interrupt		11
	interrupt		12
	interrupt		13
	interrupt		14
	interrupt		15
	interrupt		16
	interrupt		17
	interrupt		18
	interrupt		19
	interrupt		1a
	interrupt		1b
	interrupt		1c
	interrupt		1d
	interrupt		1e
	interrupt		1f
	irq				0
	irq				1
	irq				2
	irq				3
	irq				4
	irq				5
	irq				6
	irq				7
	irq				8
	irq				9
	irq				a
	irq				b
	irq				c
	irq				d
	irq				e
	irq				f
	interrupt		30
