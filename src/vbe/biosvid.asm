[bits 16]
[org 100h]

    jmp start
    dw  status
    dw  vbeinfo
    dw	modeinfo

start:
    mov ax, cs
    mov ds, ax
    mov es, ax
    
    ;
    ; Perform VBE check
    ;
    mov [status], word 1
    mov ax, 4f00h
    mov di, vbeinfo
    int 10h
    cmp al, 4fh
    jne .quit

    int 20h

    ;
    ; List VBE modes
    ;

    ; Load ES with segment of mode list pointer
    mov ax, [vbeinfo + 16]
    mov gs, ax
    ; Load BP with offset of mode list pointer
    mov bp, [vbeinfo + 14]
    ; Load DI ready for int 10h call
    mov di, modeinfo
    mov [status], word 2
        
.listmodes_start:
    ; Get next mode number in list
    mov cx, [gs:bp]
    cmp cx, 0ffffh
    je  .listmodes_finish

    ; Get mode information
    mov ax, 4f01h
    int 10h
    int 20h

    add bp, 2

    jmp .listmodes_start

.listmodes_finish:

    mov [status], word 3
    xor ax, ax
    int 20h

    mov [status], word 0
    test ax, ax
    jz .dont_switch_modes

    mov bx, ax
    mov ax, 4f02h
    int 10h
    ret

.dont_switch_modes:
    ret

.quit:
    mov [status + 1], byte 0ffh
    ret

    align 4

status      dw 0
vbeinfo     times 256 db 0
modeinfo    times 256 db 0
