; $Id: poweroff.asm,v 1.1.1.1 2002/12/21 09:49:34 pavlovskii Exp $

[bits 16]
[section .text]

[global _i386PowerOff]
_i386PowerOff:
    ;;mov ax, 5307h
    ;;mov cx, 3
    ;;mov bx, 1
    ;;int 15h

    ;mov ax, 5301h
    ;mov bx, 0
    ;int 15h        ; connect to real mode apm
    
    ;mov ax, 5307h
    ;mov bx, 1
    ;mov cx, 3
    ;int 15h        ; force system power down

    ;
    ; Power-off code from Menuet
    ;
    
    mov     ax,5304h    ; disconnect interface
    sub     bx,bx
    int     15h
    
    ;mov     ax,5302h    ; connect 16-bit protected mode interface
    mov     ax,5301h    ; connect real mode interface
    sub     bx,bx
    int     15h

    mov     ax,5308h    ; enable power management
    mov     bx,1
    mov     cx,bx
    int     15h

    mov     ax,530Dh    ; enable device power management
    mov     bx,1
    mov     cx,bx
    int     15h

    mov     ax,530Fh    ; engage power management
    mov     bx,1
    mov     cx,bx
    int     15h

    mov     ax,530Eh    ; driver version
    sub     bx,bx
    mov     cx,102h
    int     15h

    mov     ax,5307h    ; set power state
    mov     bx,1
    mov     cx,3
    int     15h

    mov     eax, 200h
    int     30h

[global _i386PowerOffEnd]
_i386PowerOffEnd:
