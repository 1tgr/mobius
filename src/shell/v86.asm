; $Id: v86.asm,v 1.1 2002/02/26 15:46:34 pavlovskii Exp $

bits 16

org 100h

start:
	mov ax, cs
	mov ds, ax
	mov ah, 9
	mov dx, msg
	int 21h
	
	mov dx, msg2
	int 21h

	mov ah, 1
	int 21h

	mov ah, 4ch
	int 21h

msg:
	db 'Hello, world!', 0xa, '$'
msg2:
	db 'Press any key to exit...', 0xa, '$'
