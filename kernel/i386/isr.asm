; $Id: isr.asm,v 1.1 2002/12/21 09:49:34 pavlovskii Exp $

[bits           32]

[section        .text]

[global         _SpinAcquire]
[global         _SpinRelease]
[global         _KeAtomicInc]
[global         _KeAtomicDec]
[global         _i386DoCall]
[global         _i386IdentifyCpu]

[extern         _i386Isr]
[extern         _i386DoubleFault]
[extern         _tss]

_SpinAcquire:
    push    ebp

    ; ebp = pointer to semaphore structure
    mov     ebp, [esp+8]

%ifndef __SMP__
    ; Check if the semaphore has already been acquired
    mov     eax, [ebp]
    test    eax, eax
    jnz     .2
%endif

    ; Save EFLAGS and store in semaphore
    pushfd
    pop     eax
    mov     [ebp+8], eax

    ; Disable interrupts on this CPU
    cli

.1:
    ; Keep trying to acquire semaphore
    mov     eax, 1
    xchg    eax, [ebp]
    or      eax, eax
    jnz     .1

    ; Store lock eip
    mov	    eax, [esp+4]
    mov	    [ebp+4], eax
    pop     ebp
    ret

.2:
    mov     eax, [ebp+4]
    mov     ebx, [esp+4]
    int     3
    ret

_SpinRelease:
    mov     eax, [esp+4]
    mov     dword [eax], 0
    mov     eax, [eax+8]
    push    eax
    popfd
    ret

    align   8

_KeAtomicInc:
    push    ebp
    mov     ebp, esp
    mov     edx, [ebp+8]
    lock inc dword [edx]
    mov     esp, ebp
    pop     ebp
    ret

    align   8

_KeAtomicDec:
    push    ebp
    mov     ebp, esp
    mov     edx, [esp+8]
    lock dec dword [edx]
    mov     esp, ebp
    pop     ebp
    ret

    align   8

; uint32_t i386DoCall(addr_t routine#8#, void *args#12#, uint32_t argbytes#16#);
_i386DoCall:
    push    ebp
    mov        ebp, esp
    
    push    esi
    push    edi

    mov        eax, [ebp+8]
    mov        esi, [ebp+12]
    mov        ecx, [ebp+16]
    
    sub        esp, ecx
    mov        edi, esp
    rep movsb

    call    eax
    
    add        esp, [ebp+16]
    pop        edi
    pop        esi
    
    pop        ebp

    ret

    align    8

_i386IdentifyCpu:
    push    ebx

    ;push    bp                     ; Align stack to dword
    ;mov        bp,sp
    ;and        sp,0FFFCh
    pushfd                        ; Save eflags
    cli                            ; No interrupts
    pushfd                        ; EAX = eflags
    pop        eax
    mov        ebx, eax            ; EBX = eflags
    xor        eax, 40000h            ; Toggle AC bit
    push    eax                    ; Eflags = EAX
    popfd
    pushfd                        ; EAX = eflags
    pop        eax
    popfd                        ; Restore eflags
    ;mov        sp,bp                ; Restore stack
    ;pop        bp
    cmp        eax,ebx                ; If the bit was not
    je        short .p1_386            ; reset, it's a 486+

    pushfd                        ; Save eflags
    cli                            ; No interrupts
    pushfd                        ; EAX = eflags
    pop        eax
    xor        eax, 200000h        ; Toggle ID bit
    push    eax                    ; Eflags = EAX
    popfd
    pushfd                        ; EBX = eflags
    pop        ebx
    popfd                        ; Restore eflags
    cmp        eax,ebx                ; If the bit was not
    jne        short .p1_486        ; reset, it's a 586+
    mov        eax, 5                ; 586+
    jmp        short .cpudone
.p1_486:
    mov        eax, 4                 ; 486
    jmp        short .cpudone
.p1_386:
    mov        edx, 3                 ; 386
    
.cpudone:
    pop        ebx
    ret

    align    8

_isr_and_switch:
    push    ds
    push    es
    push    fs
    push    gs                        ; saving segment registers and
    pushad                            ; other regs because it's an ISR

    mov         bx, 0x20                ; use the kernel selectors
    mov         ds, ebx
    mov         es, ebx
    mov         gs, ebx
    mov         bx, 0x40
    mov         fs, ebx

%ifdef __SMP__
[extern         _ThrGetCurrent]
    call        _ThrGetCurrent
%else
[extern         _thr_cpu_single]
    mov         eax, [_thr_cpu_single + 4]            ; ebx = current
%endif

    mov         eax, [eax]                ; ebx = current->ctx_last
    push        eax
    ;push    dword 0x12345678
    
    mov         ebx, esp
    push        ebx                     ; pass esp of the current thread

    call        _i386Isr                ; call scheduler
[global _isr_switch_ret]
_isr_switch_ret:
    lea     esp, [eax+4]            ; get esp of a new thread
                                    ; and skip over ctx_prev pointer

    popad                            ; restoring the regs
    pop     gs
    pop     fs
    pop     es
    pop     ds
    add        esp, 8
    iretd

    align    8

%macro        exception 1
[global        _exception_%1]

; error code already on stack

_exception_%1:
    push    dword 0x%1
    jmp     _isr_and_switch
    align    8
%endmacro

%macro        irq 1
[global        _irq_%1]

_irq_%1:
    push    dword -1    ; `fake' error code
    push    dword 0x%1
    jmp     _isr_and_switch
    align    8
%endmacro

%macro        interrupt 1
[global        _interrupt_%1]

_interrupt_%1:
    push    dword 0
    push    dword 0x%1
    jmp     _isr_and_switch
    align    8
%endmacro

    interrupt        0
    interrupt        1
    interrupt        2
    interrupt        3
    interrupt        4
    interrupt        5
    interrupt        6
    interrupt        7
    ; exception        8

[global _exception_8]
_exception_8:    ; double fault
    call    _i386DoubleFault
    add        esp, 8
    iret
    align    8

    interrupt        9
    exception        a
    exception        b
    exception        c
    exception        d
    exception        e
    interrupt        f
    interrupt        10
    interrupt        11
    interrupt        12
    interrupt        13
    interrupt        14
    interrupt        15
    interrupt        16
    interrupt        17
    interrupt        18
    interrupt        19
    interrupt        1a
    interrupt        1b
    interrupt        1c
    interrupt        1d
    interrupt        1e
    interrupt        1f
    irq                0
    irq                1
    irq                2
    irq                3
    irq                4
    irq                5
    irq                6
    irq                7
    irq                8
    irq                9
    irq                a
    irq                b
    irq                c
    irq                d
    irq                e
    irq                f
    interrupt        30
