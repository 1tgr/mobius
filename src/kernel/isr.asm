[bits		32]

[section	.text]
[extern		_isr]
[extern		_tss]
[extern		_current]

_isr_and_switch:
	push	ds
	push	es
	push	fs
	push	gs						; saving segment registers and
	pushad							; other regs because it's an ISR

	mov 	bx, 0x20				; use the kernel selectors
	mov 	ds, ebx
	mov 	es, ebx
	mov		gs, ebx
	mov		bx, 0x40
	mov		fs, ebx
	
	mov 	ebx, esp
	push	ebx 					; pass esp of a current process
	call	_isr					; call scheduler
	mov 	esp, eax				; get esp of a new process

	popad							; restoring the regs
	pop 	gs
	pop 	fs
	pop 	es
	pop 	ds
	add		esp, 8
	iretd

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
