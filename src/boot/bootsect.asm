;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Project: Bootstrap                                                       ;;
;; Module:  rpboot.asm                                                      ;;
;; Date:    7-3-99                                                          ;;
;; Purpose: This module contains the portion of the bootstrap that loads    ;;
;;          the kernel loader into memory. It is based on a bootstrap       ;;
;;          examples by Gareth Owen and Sean Tash.                          ;;
;;                                                                          ;;
;;                         Created by Ben L. Titzer                         ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

[BITS 16]                       ; bios starts out in 16-bit real mode
[ORG 0x7c00]                    ; data offset of zero

%define LOAD_SEGMENT   0x1000   ; load the loader to segment 0x1000
%define STACK_SEGMENT  0x9000   ; stack segment

jmp short start                 ; jump to beginning of code
nop

;============================================================================;
;============================ F A T   I N F O ===============================;
;============================================================================;

bsOEM       db "-Möbius-"               ; OEM String
bsSectSize  dw 512                      ; Bytes per sector
bsClustSize db 1                        ; Sectors per cluster
bsRessect   dw 1                        ; # of reserved sectors
bsFatCnt    db 2                        ; # of fat copies
bsRootSize  dw 224                      ; size of root directory
bsTotalSect dw 2880                     ; total # of sectors if < 32 meg
bsMedia     db 0xF0                     ; Media Descriptor
bsFatSize   dw 9                        ; Size of each FAT
bsTrackSect dw 18                       ; Sectors per track
bsHeadCnt   dw 2                        ; number of read-write heads
bsHidenSect dd 0                        ; number of hidden sectors
bsHugeSect  dd 0                        ; if bsTotalSect is 0 this value is
                                        ; the number of sectors
bsBootDrv   db 0                        ; holds drive that the bs came from
bsReserv    db 0                        ; not used for anything
bsBootSign  db 29h                      ; boot signature 29h
bsVolID     dd 0                        ; Disk volume ID also used for temp
                                        ; sector # / # sectors to load
bsVoLabel

  root_strt  db "  "                    ; volume label is used internally
  root_scts  db "  "                    ; while in memory for storage
  file_strt  db "  "                    ; of other variables.
  file_scts  db "  "                    ; this is just done to save space.
             db "  "
  rs_fail    db " "

bsFSType    db "FAT12   "               ; File System type

;============================================================================;

  filename   db 'MOBEL_PECOM'
  rebootmsg  db 'Press any key',13,10,0
  diskerror  db "Disk error.",0
  loadmsg    db "Loading...",0


;============================================================================;
;====================== B O O T S E C T O R    C O D E ======================;
;============================================================================;

  start:

    cli                       ; clear interrupts while we setup a stack
    mov [bsBootDrv], dl       ; save what drive we booted from
    mov ax,STACK_SEGMENT      ; setup stack segment
    mov ss,ax
    mov sp,0xffff             ; use the whole segment

;    mov cx,[bsTrackSect]      ; update int 1E FDC param table
;    mov bx,0x0078
;    lds si,[ds:bx]
;    mov byte [si+4], cl
;    mov byte [si+9], 0x0F

    sti                       ; put interrupts back on

    push ds
    mov dl,[bsBootDrv]          ; reset controller
    xor ax,ax
    int 0x13
    pop ds
    jc bootfail2                ; display error message
    jmp cont
bootfail2: jmp bootfail
cont:
    xor ax, ax                ; clear ax
    mov ds, ax                ; set up data segment accordingly
    mov es, ax
    
    mov si,loadmsg              ; display load message
    call putstr


    xor ax,ax                   ; find the root directory
    mov al,[bsFatCnt]
    mov bx,[bsFatSize]
    mul bx
    add ax,word [bsHidenSect]
    adc ax,word [bsHidenSect+2]
    add ax,word [bsRessect]     ; ax holds root directory location
    mov word [root_strt],ax

    call findfile

    xor ax,ax
    add ax,word [root_scts]
    add ax,word [file_strt]     ; sector number
    add ax,word [root_strt]
    sub ax,2                    ; correction for a mis-calc
    mov cx,word [file_scts]     ; number of sectors

    mov bx,LOAD_SEGMENT+10h     ; the 10h is because it's a com file
    mov es,bx

nextsector:
    push ax                     ; save registers
    push cx
    push dx
    push es

    xor bx,bx                   ; set zero offset
    call readsect               ; read a sector

    pop es                      ; restore registers
    pop dx
    pop cx
    pop ax
    mov bx,es
    add bx,20h                  ; increment address 512 bytes
    mov es,bx
    inc ax                      ; read next sector
    loopnz nextsector

    mov ax,LOAD_SEGMENT         ; set segment registers and jump
    mov es,ax
    mov ds,ax
    push ax                     ; push the segment
    mov ax,100h                 ; its a com file, execution starts at 100h
    push ax                     ; push offset
    retf                        ; call the second stage
findfile:
    push ax                     ; save registers
    push bx
    push cx
    push dx
    push si
    push di

    mov ax,LOAD_SEGMENT         ; put root directory at LOAD_SEGMENT
    mov es,ax
    mov ax,32                   ; AX = ((32*RootSize)/512) + 2
    mul word [bsRootSize]
    div word [bsSectSize]
    mov cx,ax                   ; cx holds # of sectors in root
    mov word [root_scts],ax
    mov ax,word [root_strt]     ; get prev. saved loc. for root dir

searchrootsect:
    xor bx,bx
    push cx                     ; save count
    push ax                     ; save sector number
    push es
    push dx
    call readsect
    xor bx,bx
checkentry:
    mov di,bx                   ; set address to check from
    mov cx,11                   ; check 11 bytes
    mov si,filename             ; address of string to check with
    repz cmpsb                  ; check string against filename
    je foundit                  ; if equal, we found it
    add bx,32                   ; otherwise, check next entry
    cmp bx,[bsSectSize]         ; end of sector?
    jne checkentry              ; if not end, continue

    pop dx                      ; restore registers
    pop es
    pop ax
    pop cx
    inc ax                      ; check next sector when we loop again
    loopnz searchrootsect       ; loop until either found or not
    jmp bootfail                ; couldn't find it: die

foundit:
    pop dx                      ; get these off the stack
    pop es
    pop ax
    pop cx

    mov di,0x1A                 ; get clustor #
    add di,bx
    push bx                     ; save bx for finding # of sectors
    mov ax,[es:di]
    xor bx,bx                   ; calculate sector #
    mov bl,[bsClustSize]
    mul bx                      ; ax holds sector #
    mov word [file_strt],ax

    pop bx                      ; get location of directory entry
    mov di,0x1C
    add di,bx
    mov ax,[es:di]              ; put number of bytes in ax
    xor dx,dx
    mov bx,[bsSectSize]         ; # of bytes / 512
    div bx
    inc ax
    mov word [file_scts],ax     ; save number of sectors to load

    pop di                      ; restore registers
    pop si
    pop dx
    pop cx
    pop bx
    pop ax

    ret                         ; return to caller

  bootfail:
    mov si,diskerror            ; display error message
    call putstr
    call reboot



;============================================================================;
;=========================== F U N C T I O N S ==============================;
;============================================================================;

  readsect:                     ; ES:BX = Location ; AX = Sector
    mov [rs_fail], byte 0       ; no failures yet

  readsect2:

    push ax                     ; store sector

    mov si,[bsTrackSect]
    div si                      ; divide logical sect by track size
    inc dl                      ; sector # begins at 1
    mov [bsReserv],dl           ; sector to read
    xor dx,dx                   ; logical track left in ax
    div word [bsHeadCnt]        ; leaves head in dl, cyl in ax
    mov dh, [bsBootDrv]         ;
    xchg dl,dh                  ; head to dh, drive to dl
    mov cx,ax                   ; cyl to cx
    xchg cl,ch                  ; low 8 bits of cyl to ch, hi 2 bits
    shl cl,6                    ; shifted to bits 6 and 7
    or cl, byte [bsReserv]      ; or with sector number
    mov al,1                    ; number of sectors
    mov ah,2                    ; use read function of int 0x13
    int 0x13                    ; read sector
    jc readfail                 ; display error message
    pop ax
    ret                         ; return to caller

  readfail:                     ; read failed
    inc byte [rs_fail]
    cmp byte [rs_fail], 4      ; stop at 4 tries
    je bootfail

    mov dl,[bsBootDrv]          ; reset controller
    xor ax,ax
    int 0x13
    pop ax
    jmp readsect2



;============================================================================;

%ifdef PUTINT
  putint:                     ; print ax to screen

    mov si, di                ; if di is set, then print leading zeroes
    mov bx, 10000
    mov cx, ax                ; store a copy of ax

  pi1:
    cmp bx, 1                 ; if we are on last digit
    jne cn
    mov si, 1                 ; make sure we print it (even if zero)
  cn:
    mov ax, cx                ; get current num
    xor dx, dx                ; clear dx
    div bx                    ; divide by decimal
    mov cx, dx                ; save remainder
    or si, ax                 ; if si and ax are zero, it's a leading zero
    jz  nz
    add ax, '0'               ; make a character
    mov ah, 0eh               ; print character function
    int 0x10                  ; call BIOS

  nz:
    mov ax, bx                ; we have to reduce the radix too
    xor dx, dx                ; clear dx
    mov bx, 10                ; use 10 as new divider
    div bx                    ; divide
    mov bx, ax                ; save result
    or bx, bx                 ; if zero, we're done
    jnz pi1

    ret
%endif

;============================================================================;

  putstr:                       ; Dump ds:si to screen.
    lodsb                       ; load byte at ds:si into al
    or al,al                    ; test if character is 0 (end)
    jz done
    mov ah,0eh                  ; put character
    mov bx,0009                 ; attribute
    int 0x10                    ; call BIOS
    jmp putstr
  done:
    ret

;============================================================================;

  reboot:
    mov si, rebootmsg           ; be polite, and say we're rebooting
    call putstr
    mov ah, 0                   ; subfuction 0
    int 016h                    ; call bios to wait for key

    db 0EAh                     ; machine language to jump to FFFF:0000 (reboot)
    dw 0000h
    dw 0FFFFh


;============================================================================;
;============================ E N D   C O D E ===============================;
;============================================================================;


padding    times 510-($-$$) db 0     ; make the file the right size
BootSig    dw 0xAA55                 ; magic word for BIOS

;============================================================================;

