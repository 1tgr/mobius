;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Project: Second Stage Bootloader 										;;
;; Module:	minifs.asm														;;
;; Date:	02-21-2000														;;
;; Purpose: This module contains the portion of the bootstrap that handles	;;
;;			filesystem input and ouput in order to load the kernel. 		;;
;;																			;;
;;						   Created by Ben L. Titzer 						;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;fs_name   db 'VFAT',0		   ; name of filesystem we are using
;fs_hname  db 'VFAT	  RFS'	   ; name of secondary manager to load

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;						 B O O T   S E C T O R								;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

bootsect db 0,0,0

bsOEM		db "ok ok ok"				; OEM String
bsSectSize	dw 512						; Bytes per sector
bsClustSize db 1						; Sectors per cluster
bsRessect	dw 1						; # of reserved sectors
bsFatCnt	db 2						; # of fat copies
bsRootSize	dw 224						; size of root directory
bsTotalSect dw 2880 					; total # of sectors if < 32 meg
bsMedia 	db 0xF0 					; Media Descriptor
bsFatSize	dw 9						; Size of each FAT
bsTrackSect dw 18						; Sectors per track
bsHeadCnt	dw 2						; number of read-write heads
bsHidenSect dd 0						; number of hidden sectors
bsHugeSect	dd 0						; if bsTotalSect is 0 this value is
										; the number of sectors
bsBootDrv	db 0						; holds drive that the bs came from
bsReserv	db 0						; not used for anything
bsBootSign	db 29h						; boot signature 29h
bsVolID 	dd 0						; Disk volume ID also used for temp
										; sector # / # sectors to load
bsVoLabel	db "           "			; Volume Label
bsFSType	db "FAT12   "				; File System type

bs_padding times 512-($-bootsect) db 0	; padding

root_strt	dw 0			; start of root directory
root_scts	dw 0			; num sectors of root directory
file_strt	dw 0			; start of file
file_scts	dw 0			; num sectors of file
tmpname 	dw 0			; temporary name of file to load

debug	   db ' - ',0
dot 	   db '.',0
xstr	   db 'x',0
rsfail	   db 'readsect failed!',0
fsinitfail db 'intiatilization failed!',0


;============================================================================;

fs_init:		   ; initialize filesystem

	push ds
	mov dl,[boot_drv]			; reset controller
	xor ax,ax
	int 0x13
	pop ds
	jc fs_init_fail 			; failed to initialize device

	xor ax,ax					; find start of root directory
	mov al,[bsFatCnt]			; skip any FATs
	mov bx,[bsFatSize]
	mul bx						; have to multiply by FAT size
	add ax,word [bsHidenSect]	; add any hidden sectors
	adc ax,word [bsHidenSect+2]
	add ax,word [bsRessect] 	; add any reserved sectors
	mov word [root_strt],ax 	; store sector # of root directory

	mov ax,32					; find size in sectors of root dir
	mul word [bsRootSize]		; ((32*RootSize)/512) + 2
	div word [bsSectSize]
	mov word [root_scts],ax 	; store number of sectors in root dir

	xor ax, ax					; we have to load sector 0
	mov bx, ds
	mov es, bx
	mov bx, bootsect
	call fs_readsect			; read sector 0, bootsector
	cmp al, FS_OK				; did it work?
	jne fs_init_fail			; nope, report error

	mov al, FS_OK				; signal success
	ret 						; return to caller

fs_init_fail:
	mov al, FS_ERROR			; signal that it didn't work
	ret

;============================================================================;

fs_loadfail1:
	mov ax, FS_ERROR
	ret

fs_loadfile:		  ; load file name with name at ds:si into ebx ; es:di

	call fs_rootfind			; try to find the file in root dir
	cmp ax, FS_OK				; if it is 0, then not found
	jne fs_loadfail1 			; failed to locate file

	xor eax,eax
	add ax,word [root_scts]
	add ax,word [file_strt] 	; sector number
	add ax,word [root_strt]
	sub ax,2					; correction for a mis-calc
	mov cx,word [file_scts] 	; number of sectors
	mov dl,[boot_drv]

	push es
	push edi
	push esi
	push bp
	call load
	pop bp
	pop esi
	pop edi
	pop es

	mov ax, FS_OK
	ret

sectors_total:	dw	0

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
;*	bx, es, edi, esi, ecx, bp
;**************************************

load:
	mov		si, BUFFER_ADDR >> 4
	mov		es, si
	mov		di, 1
	xor		bp, bp
	mov		[sectors_total], cx
	
load_loop:
	push	ebx
	call	read_sectors
	pop		ebx
	
	push	cx
	push	di
	push	es
	push	ds
	
	xor		ecx, ecx
	mov		es, cx
	mov		ds, cx
	;movzx	ecx, word [bsSectSize]
	mov		ecx, 512
	mov		edi, ebx
	add		ebx, ecx
	mov		esi, BUFFER_ADDR

	;cli
	;hlt

	a32 rep	movsb

	pop		ds
	pop		es
	pop		di
	pop		cx

	;mov		bx, es
	;add		bx, 32
	;mov		es, bx

	inc		ax

	inc		bp

	push	ax
	push	bx
	
	mov		ax, [sectors_total]
	mov		bx, 80
	div		bx
	xor		ah, ah
	cmp		bp, ax
	jle		.1

	;push	bp
	
	mov		ax, 0efeh
	xor		bx, bx
	int		10h

	;pop		bp
	xor		bp, bp
	
.1:
	pop		bx
	pop		ax

	loop	load_loop
	ret

%if 0

loadloop:
	push ax 					; save registers
	push cx
	push dx
	push es
	push di 					; save this offset

	mov bx,di					; use offset we were given
	call fs_readsect			; read a sector
	mov [temp], ax
	
	pop di
	pop es						; restore registers
	pop dx
	pop cx
	pop ax

	mov ax, [temp]
	or ax, ax
	jnz fs_loadfail

	push ax
	push bx
	mov ax, 0e00h | '.'
	xor bx, bx
	int 10h
	;mov si, dot
	;call putstr
	pop bx
	pop ax

	mov bx,es
	add bx,20h					; increment address 512 bytes
	mov es,bx
	inc ax						; read next sector
	loopnz loadloop 			; loop until cx is zero

	mov ax, FS_OK
	ret

%endif

fs_loadfail:
	mov ax, FS_ERROR
	ret

;============================================================================;

fs_rootfind:		  ; searches for file in root directory
	 push bx
	 push cx
	 push dx
	 push si
	 push di

	 mov ax, [root_strt]		 ; start of root directory
	 mov cx, [root_scts]		 ; number of sectors in root dir
	 dec cx 					 ; don't check the junk sector
	 xor dx, dx
	 mov [tmpname], si			 ; filename to search for

checksect:

	 mov bx, sectorbuf			 ; load shit into sector buffer
	 push cx					 ; save count
	 push ax					 ; save sector number
	 push es
	 push dx
	 call fs_readsect			 ; read the root sector
	 cmp ax, FS_OK
	 jne fs_loadfail
	 mov bx, sectorbuf			 ; use stuff in sector buffer

checkentry:
	 mov di,bx					 ; set address to check from
	 mov cx,11					 ; check 11 bytes
	 mov si,[tmpname]			 ; address of string to check with
	 repz cmpsb 				 ; check string against filename
	 je foundit 				 ; if equal, we found it
	 add bx,32					 ; otherwise, check next entry
	 cmp bx,sectorbuf_end		 ; end of sector?
	 jl  checkentry 			 ; if not end, continue

	 pop dx 					 ; restore registers
	 pop es
	 pop ax
	 pop cx
	 inc ax 					 ; check next sector when we loop again
	 loopnz checksect			 ; loop until end of root sectors
	 jmp notfound				 ; couldn't find it: die

foundit:
	 pop dx 					 ; get these off the stack
	 pop es
	 pop ax
	 pop cx

	 mov di,0x1A				 ; get clustor #
	 add di,bx
	 push bx					 ; save bx for finding # of sectors
	 mov ax,[es:di]
	 xor bx,bx					 ; calculate sector #
	 mov bl,[bsClustSize]
	 mul bx 					 ; ax holds sector #
	 mov word [file_strt],ax

	 pop bx 					 ; get location of directory entry
	 mov di,0x1C
	 add di,bx
	 mov eax, [es:di] 			 ; put number of bytes in ax
	 ;xor edx, edx
	 ;xor ebx, ebx
	 ;mov bx, word [bsSectSize]		 ; # of bytes / 512
	 ;div ebx
	 shr eax, 9
	 inc ax
	 mov word [file_scts],ax	 ; save number of sectors to load

	 pop di 					 ; restore registers
	 pop si
	 pop dx
	 pop cx
	 pop bx

	 mov ax, FS_OK				 ; we good
	 ret						 ; return to caller

notfound:
	 pop di 					 ; restore registers
	 pop si
	 pop dx
	 pop cx
	 pop bx
	 mov ax, FS_ERROR			 ; didn't find it
	 ret

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
	movzx	ebx, byte [bsTrackSect]
	div		ebx				;EAX=track ;EDX=sector-1
	mov		cx, dx			;CL=sector-1 ;CH=0
	inc		cx			;	CL=Sector number
	xor		dx, dx
	mov		bl, [bsHeadCnt]
	div		ebx

	mov		dh, dl			;Head
	mov		dl, [boot_drv]	;Drive 0
	xchg	ch, al			;CH=Low 8 bits of cylinder number; AL=0
	shr		ax, 2			;AL[6:7]=High two bits of cylinder; AH=0
	or		cl, al			;CX = Cylinder and sector
	mov		ax, di			;AX = Maximum sectors to xfer
.retry:
	mov		ah, 2			;Read
	xor		bx, bx
	int		13h
	jc		.retry

	popa

	ret

;============================================================================;

rs_fails: db 0

fs_readsect:					; ES:BX = Location ; AX = Sector

	mov [rs_fails], word 0		; start with 0 failures

fs_readsect2:
	push ax 					; save sector

	mov si,[bsTrackSect]
	div si						; divide logical sect by track size
	inc dl						; sector # begins at 1
	mov [bsReserv],dl			; sector to read
	xor dx,dx					; logical track left in ax
	div word [bsHeadCnt]		; leaves head in dl, cyl in ax
	mov dh, [boot_drv]			;
	xchg dl,dh					; head to dh, drive to dl
	mov cx,ax					; cyl to cx
	xchg cl,ch					; low 8 bits of cyl to ch, hi 2 bits
	shl cl,6					; shifted to bits 6 and 7
	or cl, byte [bsReserv]		; or with sector number
	mov al,1					; number of sectors

	mov ah,2					; use read function of int 0x13
	int 0x13					; read sector
	jc readfail 				; error
	pop ax
	mov ax, FS_OK				; success
	ret 						; return to caller

readfail:
	inc byte [rs_fails] 		; increment number of failures
	cmp byte [rs_fails], 10 	; 4th try?
	je readfail3				; if so, give up

	mov dl,[boot_drv]			; reset controller
	xor ax,ax
	int 0x13

	pop ax						; restore sector number
	jmp fs_readsect2			; retry

readfail3:
	pop ax						; fix stack
	mov ax, FS_ERROR			; signal an error
	ret 						; return

;============================================================================;
