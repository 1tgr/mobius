;
; $History$
;

[bits 16]
[org 0]

    jmp start
    
    align 4

command_event   dd  0xaabbccdd
status_event    dd  0xeeff0011
command         dw  0
param           dw  0
status          dw  0
                dw  vbeinfo
                dw  modeinfo

start:
    ; Load segment registers for a single segment
    mov ax, cs
    mov ds, ax
    mov es, ax

next_command:
    ; Wait for command_event to be signalled
    mov eax, [command_event]
    push eax
    call ThrWaitHandle
    add sp, 4

    ; Look at the command we've received
    mov ax, [command]

    test ax, ax
    jz do_exit              ; 0 => exit

    dec ax
    jz do_check             ; 1 => check VBE

    dec ax
    jz do_get_mode_info     ; 2 => get mode info

    dec ax
    jz do_set_mode          ; 3 => set mode

    mov word [status], -1   ; else => error

finish_command:
    ; Signal the event that says we've finished
    push dword 1
    mov eax, [status_event]
    push eax
    call EvtSignal
    add esp, 4

    ; Get the next command
    jmp next_command


do_exit:
    push dword [param]
    call ThrExitThread
    add sp, 4
    int 3
    jmp next_command


do_check:
    ;
    ; Perform VBE check
    ;
    mov byte [vbeinfo+0], 'V'
    mov byte [vbeinfo+1], 'E'
    mov byte [vbeinfo+2], 'S'
    mov byte [vbeinfo+3], 'A'
    
    mov ax, 4f00h
    mov di, vbeinfo
    int 10h
    mov [status], ax
    jmp finish_command


do_get_mode_info:
    ;
    ; Get mode information
    ;

    mov cx, [param]
    mov di, modeinfo
    mov ax, 4f01h
    int 10h
    mov [status], ax
    jmp finish_command


do_set_mode:
    ;
    ; Set mode
    ;
    mov bx, [param]
    mov ax, 4f02h
    int 10h
    jmp finish_command

    align 4


ThrExitThread:
    mov ax, 200h
    mov cx, 4
    mov dx, sp
    add dx, 2
    int 30h
    ret

    align 4

ThrWaitHandle:
    mov ax, 201h
    mov cx, 4
    mov dx, sp
    add dx, 2
    int 30h
    ret

    align 4

EvtSignal:
    mov ax, 602h
    mov cx, 8
    mov dx, sp
    add dx, 2
    int 30h
    ret

    align 4

vbeinfo     times 256 db 0
modeinfo    times 256 db 0
