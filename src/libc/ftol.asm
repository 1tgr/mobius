[bits 32]

; [global __ftol]

; __ftol:
	push        ebp
	mov         ebp,esp
	add         esp,0F4h
	wait
	fnstcw      word [ebp-2]
	wait
	mov         ax,word [ebp-2]
	or          ah,0Ch
	mov         word [ebp-4],ax
	fldcw       word [ebp-4]
	fistp       qword [ebp-0Ch]
	fldcw       word [ebp-2]
	mov         eax,dword [ebp-0Ch]
	mov         edx,dword [ebp-8]
	leave
	ret
