; GazOS Operating System
; Copyright (C) 1999  Gareth Owen <gaz@athene.co.uk>
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation; either version 2
; of the License, or (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


%include "gdtnasm.inc"
; GazOS bootsector by Gareth Owen <gaz@athene.co.uk>

%define	ONE_MEG		0x100000
%define MEM_INCR	ONE_MEG
%define LOAD_ADDR	ONE_MEG
%define RDSK_ADDR	(ONE_MEG * 2)
%define BUFFER_ADDR	0x8000
%define KERNEL_SIZE	(kernel_end - kernel_start)
%define RDSK_SIZE	(ramdisk_end - kernel_end)

[ORG 0x7c00]
jmp start
nop

;START BOOTSECTOR HEADER

;id				db	'GazFS'	; Filing System ID
;version			dd	1h 		; Filing System Version
;fs_start		dd	52		; LBA address for start of root dir

BytesPerSector	dw	512
SectorsPerTrack	dw	18
TotalHeads		dw	2
TotalSectors	dd	2880	 ; 1474560/512 for a 1.44meg disk

;END BOOTSECTOR HEADER

;BOOTSECTOR DATA
file_entry_nextdata	equ	273	; Offset in file_entry structure to the nextdata LBA pointer
data_entry_data		equ	9	; Offset in data_entry structure to the data
bootdrv		db	0

;END BOOTSECTOR DATA

start:
xor ax, ax
mov ds, ax
mov [bootdrv], dl

; First get into protected mode
cli				;{0}
n5:	in		al, 0x64		;Enable A20 {4A} {5}
	test	al, 2
	jnz		n5
	mov		al, 0xD1
	out		0x64, al
n6:	in		al, 0x64
	test	al, 2
	jnz		n6
	mov		al, 0xDF
	out		0x60, al
	lgdt    [gdtinfo]               ;Load GDT
	mov		ecx, CR0		;Switch to protected mode
	inc		cx
	mov		CR0, ecx

	mov		ax, flat_data-gdt_table	; Selector for 4Gb data seg
	mov		ds, ax			; {2} Extend limit for ds
	mov		es, ax			; Extend limit for es
	mov		fs, ax			; fs and...
	mov 	gs, ax			; gs
	dec		cx				; switch back to real mode
	mov		CR0, ecx

	sti

	xor		eax, eax
	mov		ds, ax

	mov		dl, [bootdrv]		; Store the boot drive
	mov		ax, 1				; sector number
	
	mov		cx, KERNEL_SIZE / 512; total sectors
	mov		ebx, LOAD_ADDR		; destination in memory
	call	load
	mov		cx, RDSK_SIZE / 512
	mov		ebx, RDSK_ADDR
	call	load
	jmp		count_memory
	
;**************************************
;* in:
;*	eax: start sector
;*	ebx: start of destination buffer
;*	 cx: number of sectors
;*	 dl: disk drive
;* out:
;*	 ax: end sector
;*	ebx: end of destination buffer
;* modifies:
;*	bx, es, edi, esi, ecx
;**************************************

load:
	mov		si, BUFFER_ADDR >> 4
	mov		es, si
	mov		di, 1
load_loop:
	push	ebx
	call	read_sectors
	pop		ebx
	
	push	cx
	push	di
	push	es
	
	xor		cx, cx
	mov		es, cx
	movzx	ecx, word [BytesPerSector]
	mov		edi, ebx
	add		ebx, ecx
	mov		esi, BUFFER_ADDR
	a32 rep	movsb

	pop		es
	pop		di
	pop		cx

	;mov		bx, es
	;add		bx, 32
	;mov		es, bx

	inc		ax
	loop	load_loop
	ret

count_memory:
	; Turn off the floppy motor, its annoying leaving it on !
        mov edx,0x3f2
        mov al,0x0c
        out dx,al
	; set, restore and test a byte at each 1 Mb boundary
	; the end is reached when byte read != byte written
	mov     edi,0
.start:
	mov     al,byte [es:edi]
	mov     byte [es:edi],0aah
	mov     ah,byte [es:edi]
	mov     byte [es:edi],al
	cmp     ah,0aah
	jne     .finish
	add     edi,MEM_INCR
	jmp     .start

.finish:
	mov     [memory_top],edi
	
; Re-enter protected mode ! A20 is already enabled
	cli		; No interrupts please at all
	lgdt	[gdtinfo]
	mov		ecx, cr0
	inc		cx
	mov		cr0, ecx
	mov		ax, flat_data-gdt_table
	mov		fs, ax
	mov		gs, ax
	mov		ax, kernel_data-gdt_table
	mov		ds, ax
	mov		es, ax
	mov		ss, ax

	jmp		dword (flat_code-gdt_table):pmode1

read_sectors:
; Input:
;	EAX = LBN
;	DI  = sector count
;	ES = segment
; Output:
;	BL = low byte of ES
;	EBX high half cleared
;	DL = 0x80
;	EDX high half cleared
;	ESI = 0

	pusha

	cdq						;edx = 0
	movzx	ebx, byte [SectorsPerTrack]
	div		ebx				;EAX=track ;EDX=sector-1
	mov		cx, dx			;CL=sector-1 ;CH=0
	inc		cx			;	CL=Sector number
	xor		dx, dx
	mov		bl, [TotalHeads]
	div		ebx

	mov		dh, dl			;Head
	mov		dl, [bootdrv]	;Drive 0
	xchg	ch, al			;CH=Low 8 bits of cylinder number; AL=0
	shr		ax, 2			;AL[6:7]=High two bits of cylinder; AH=0
	or		cl, al			;CX = Cylinder and sector
	mov		ax, di			;AX = Maximum sectors to xfer
retry:
	mov		ah, 2			;Read
	xor		bx, bx
	int		13h
	jc		retry

	popa

	ret

[BITS 32]
pmode1:
	
	push	dword 2
	popfd

	mov		eax, dword[fs:memory_top]
	mov		ebx, KERNEL_SIZE
	mov		ecx, LOAD_ADDR
	mov		edx, RDSK_SIZE
	mov		esi, RDSK_ADDR

	;jmp		$
	jmp		(kernel_code-gdt_table):0

gdtinfo:

dw	gdtlength
dd	gdt_table

;********* GDT TABLE
gdt_table:

null_desc	desc	0,0,0
flat_code	desc	0, 0xFFFFF, D_CODE + D_READ + D_BIG + D_BIG_LIM
flat_data	desc	0, 0xFFFFF, D_DATA + D_WRITE + D_BIG + D_BIG_LIM
kernel_code	desc	LOAD_ADDR, 0xFFFFF, D_CODE + D_READ + D_BIG + D_BIG_LIM
kernel_data	desc	LOAD_ADDR, 0xFFFFF, D_DATA + D_WRITE + D_BIG + D_BIG_LIM

gdtlength equ $ - gdt_table - 1
;********* END GDT TABLE
memory_top:	dd		0

times 510-($-$$) nop
dw 0xAA55

kernel_start:
incbin	"kernel.bin"
kernel_end:
incbin	"ramdisk.bin"
ramdisk_end: