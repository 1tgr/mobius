; $Id: v86.asm,v 1.1 2002/12/21 09:51:10 pavlovskii Exp $

bits 16

org 100h

start:
	mov ax, cs
	mov ds, ax
	mov ah, 9
	mov dx, msg
	int 0x21
	
	xor bx, bx
	mov dx, msg2
.loop:
	lea cx, ['0' + bx]
	mov [num], cl
	int 0x21
	inc bx
	cmp bx, 10
	jne .loop

	;mov ah, 1
	;int 21h

	mov ah, 0x4c
	int 0x21

msg:
	db 'Hello, world!', 0xa, '$'
msg2:
	db 'Count: '
num:
	db '0', 0xa, '$'
