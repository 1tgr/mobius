	.286
	ifndef	??version
?debug	macro
	endm
$comm	macro	name,dist,size,count
	comm	dist name:BYTE:count*size
	endm
	else
$comm	macro	name,dist,size,count
	comm	dist name[size]:BYTE:count
	endm
	endif
	?debug	S "mobel_pe.c"
	?debug	C E94A75092B0A6D6F62656C5F70652E63
_TEXT	segment byte public 'CODE'
_TEXT	ends
DGROUP	group	_DATA,_BSS
	assume	cs:_TEXT,ds:DGROUP
_DATA	segment word public 'DATA'
d@	label	byte
d@w	label	word
_DATA	ends
_BSS	segment word public 'BSS'
b@	label	byte
b@w	label	word
_BSS	ends
_DATA	segment word public 'DATA'
	db	72
	db	101
	db	108
	db	108
	db	111
	db	44
	db	32
	db	119
	db	111
	db	114
	db	108
	db	100
	db	33
	db	0
_DATA	ends
_TEXT	segment byte public 'CODE'
	assume	cs:_TEXT
_main	proc	near
	enter	14,0
	push	ss
	lea	ax,word ptr [bp-14]
	push	ax
	push	ds
	push	offset DGROUP:d@+0
	mov	cx,14
	call	near ptr N_SCOPY@
	mov	 ax, 1301h
	mov	 bx, 0007h
	mov	 cx, sizeof([bp-14])
	mov	 dx, 0
	mov	 bp, offset [bp-14]
@1@170:
	jmp	short @1@170
	leave	
	ret	
_main	endp
_TEXT	ends
_BSS	segment word public 'BSS'
__ext_mem_size	label	word
	db	4 dup (?)
__got_32bit_cpu	label	byte
	db	1 dup (?)
	db	1 dup (?)
__conv_mem_size	label	word
	db	4 dup (?)
	?debug	C E9
_BSS	ends
_DATA	segment word public 'DATA'
s@	label	byte
_DATA	ends
_TEXT	segment byte public 'CODE'
_TEXT	ends
	public	__conv_mem_size
	public	__got_32bit_cpu
	public	_main
	extrn	N_SCOPY@:far
	public	__ext_mem_size
	end
