section		.text	code
bits		32

[global		_NtProcessStartup]
[extern		__main]
[extern		_scode]
[extern		__sysinfo]
_NtProcessStartup:
	;            eax -> address at top of memory
	;            ebx -> size of ramdisk
	;            ecx -> physical address of start
	;            edx -> size of ramdisk
	;            esi -> physical address of ramdisk
	; cs, ds, es, ss -> base = start
	;             fs -> base = start of loader (.COM program)
	;             gs -> linear (base = 0)

	mov		di,ds
	mov		ss,di
	mov		esp,_stack_end
	sub		esp,_scode
	
	mov		edi,_temp
	sub		edi,_scode
	mov		[edi+0x0],eax
	mov		[edi+0x4],ebx
	mov		[edi+0x8],ecx
	mov		[edi+0xc],edx
	mov		[edi+0x10],esi

[extern     _sbss]
[extern     _ebss]
	xor		eax,eax
	mov		edi,_sbss
	sub		edi,_scode
	mov     ecx,_ebss
	sub     ecx,_sbss

	rep     stosb

	mov		edi,_temp
	sub		edi,_scode
	push	dword [edi+0x0]
	push	dword [edi+0x4]
	push	dword [edi+0x8]
	push	dword [edi+0xC]
	push	dword [edi+0x10]

	call	__main

	cli
	hlt

[global		___main]
___main:
	ret

[global		_i386_llmemcpy]
_i386_llmemcpy:
	push	ebp
	mov		ebp,esp
	push	edi
	;push	eax
	push	ecx
	
	mov		edi,[ebp+8]
	mov		eax,edi
	mov		esi,[ebp+12]
	mov		ecx,[ebp+16]
	gs repnz	movsb

	pop		ecx
	;pop		eax
	pop		edi
	pop		ebp
	ret

[global		_i386_docall]
_i386_docall:
	; This code blatantly nicked from MFC OLECALL.CPP
	pop     edx         ; edx = return address
	pop     eax         ; eax = pfn
	pop     ecx         ; ecx = pArgs
	add     ecx,[esp]   ; ecx += nSizeArgs (=scratch area)
	mov     [ecx],edx   ; scratch[0] = return address
	sub     ecx,[esp]   ; ecx = pArgs (again)
	mov     esp,ecx     ; esp = pArgs (usually already correct)
	pop     ecx         ; ecx = this pointer (the CCmdTarget*)
	call    eax         ; call member function
	ret                 ; esp[0] should = scratch[0] = return address

[bits		16]
[global		_code86]
[global		_code86_end]
_code86:
	mov		ax, cs
	mov		ds, ax
	mov		es, ax
	mov		fs, ax
	mov		gs, ax
	mov		ss, ax

	mov		cx, 10
	
.1:
	mov		ax, 4
	int		30h
	loop	.1

	mov		ax, 13h
	int		10h

	mov		ax, 106h
	int		30h
	
	;mov		ax, 1300h
	;xor		bh,bh
	;mov		bl,15
	;mov		cx,13
	;mov		dx,1010h
	;mov		bp,s - _code86
	;int		10h
	;inc		bl
	;int		79h

;s:
	;db		"Hello, world!"

;	mov		ax, 12h
;    int		10h
;
;	mov		bx, 7
;	mov		ax, 0E00h
;.1:	
;	int		10h
;	inc		al
;	jmp		.1

;	mov		ax, 'E'+0x0E00
;	mov		bx, 7
;	int		10h
;	mov		ax,0xb800
;	mov		es,ax
;	xor		di,di
;	mov		ax,[.data86 - _code86]
;	es mov	[di],ax
;	mov		cx,40
;.1:
;	es mov	ax,[di]
;	inc		ax
;	es mov	[di],ax
;	add		di,2
;	cmp		di,80*25*2
;	jge		.2
;	loop	.1
;	int		79h
;.2:
;	xor		di,di
;	jmp		.1
_code86_end:

section		.data	data
_temp:		times 8 dd		0
	
;section		.bss	bss
[extern		_stack]
;_stack:		resb	8192
[extern		_stack_end]
;_stack_end:	resd	1

[extern		_kernel_pagedir]
;_kernel_pagedir:	resb	4096
