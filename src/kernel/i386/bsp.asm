; $Id: bsp.asm,v 1.2 2002/08/06 11:02:57 pavlovskii Exp $

; Definitions from John Fine's gdtnasm.inc

;Each descriptor should have exactly one of next 8 codes to define the type of
;descriptor
D_LDT		EQU	 200h	;LDT segment
D_TASK		EQU	 500h	;Task gate
D_TSS		EQU	 900h	;TSS
D_CALL		EQU	0C00h	;386 call gate
D_INT		EQU	0E00h	;386 interrupt gate
D_TRAP		EQU	0F00h	;386 trap gate
D_DATA		EQU	1000h	;Data segment
D_CODE		EQU	1800h	;Code segment

;Descriptors may include the following as appropriate:
D_DPL3		EQU	6000h	;DPL3 or mask for DPL
D_DPL2		EQU	4000h
D_DPL1		EQU	2000h
D_PRESENT	EQU	8000h	;Present
D_NOT_PRESENT	EQU	8000h	;Not Present
				;Note, the PRESENT bit is set by default
				;Include NOT_PRESENT to turn it off
				;Do not specify D_PRESENT

;Segment descriptors (not gates) may include:
D_ACC		EQU	 100h	;Accessed (Data or Code)

D_WRITE		EQU	 200h	;Writable (Data segments only)
D_READ		EQU	 200h	;Readable (Code segments only)
D_BUSY		EQU	 200h	;Busy (TSS only)

D_EXDOWN	EQU	 400h	;Expand down (Data segments only)
D_CONFORM	EQU	 400h	;Conforming (Code segments only)

D_BIG		EQU	  40h	;Default to 32 bit mode (USE32)
D_BIG_LIM	EQU	  80h	;Limit is in 4K units


[section    .text]

[global     _i386MpApInit]
[global     _i386MpApInitData]
[global     _i386MpApInitEnd]
[extern     _i386MpApEntry]

start:
_i386MpApInit:
    ; The real-mode part of this routine is located in low memory
[bits       16]
    mov     eax, 0x00000100
    mov     ds, ax

    ; Place the stack 4096 bytes after the start of the code
    mov     ax, cs
    shl     ax, 4
    add     ax, 0x1000
    mov     esp, eax

    ; Load ebx with the CPU ID for the call to i386MpApEntry later
    mov     ebx, [id - start]

    ; Load GDT
    lgdt    [gdtr - start]

    ; Load page directory
    mov     eax, [pdbr - start]
    mov     cr3, eax

    ; Set protected mode and paging
    mov     eax, cr0
    or      eax, 0x80000001
    mov     cr0, eax

    ; This is a far JMP, to the real i386MpApinit.pmode in kernel space
    jmp     dword 8:.pmode

    align 8

.pmode:
[bits       32]
    mov     ax, 16
    mov     ss, eax
    mov     ds, eax
    mov     es, eax
    mov     fs, eax
    mov     gs, eax

    push    ebx
    call    _i386MpApEntry

    cli
    hlt

    align 8

_i386MpApInitData:

pdbr:   dd  0xdeadbeef
id:     dd  0
gdt:
gdtr:
    ; NULL
    dw      gdt_end - gdt - 1
    dd      gdt - start + 0x1000
    dw      0
    ; code
	dw	0xffff  ; limit 15..0
	dw	0       ; base 15..0
    db	0       ; base 23..16
	db	(D_CODE | D_READ | D_PRESENT) >> 8
	db	D_BIG | D_BIG_LIM | 0x0f
	db	0       ; base 32..24
    ; data
	dw	0xffff  ; limit 15..0
	dw	0       ; base 15..0
    db	0       ; base 23..16
	db	(D_DATA | D_WRITE | D_PRESENT) >> 8
	db	D_BIG | D_BIG_LIM | 0x0f
	db	0       ; base 32..24
gdt_end:

_i386MpApInitEnd:
