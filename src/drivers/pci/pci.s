	.file	"pci.c"
gcc2_compiled.:
___gnu_compiled_c:
.text
	.align 4
.globl _pciRead
	.def	_pciRead;	.scl	2;	.type	32;	.endef
_pciRead:
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	orl $-2147483648,%ecx
	xorl %eax,%eax
	andl $-2147418113,%ecx
	xorl %edx,%edx
	pushl %ebx
	movl 24(%ebp),%ebx
	movb 8(%ebp),%al
	sall $16,%eax
	orl %eax,%ecx
	movb 12(%ebp),%dl
	andl $31,%edx
	sall $11,%edx
	andb $7,%ch
	orl %edx,%ecx
	xorl %eax,%eax
	movb 16(%ebp),%al
	andl $7,%eax
	sall $8,%eax
	andb $248,%ch
	orl %eax,%ecx
	movb 20(%ebp),%dl
	andb $-4,%dl
	movb %dl,-4(%ebp)
	movb %dl,%cl
	movl $3320,%edx
	movl %ecx,%eax
/APP
	outl	%eax, %dx
/NO_APP
	movl 20(%ebp),%ecx
	andl $3,%ecx
	addw $3324,%cx
	cmpl $2,%ebx
	je L23
	jg L29
	cmpl $1,%ebx
	je L21
	jmp L27
	.p2align 4,,7
L29:
	cmpl $4,%ebx
	je L25
	jmp L27
	.p2align 4,,7
L21:
	movl %ecx,%edx
/APP
	inb	%dx,%al
/NO_APP
	andl $255,%eax
	jmp L30
	.p2align 4,,7
L23:
	movl %ecx,%edx
/APP
	inw	%dx,%ax
/NO_APP
	andl $65535,%eax
	jmp L30
	.p2align 4,,7
L25:
	movl %ecx,%edx
/APP
	inl	%dx,%eax
/NO_APP
	jmp L30
	.p2align 4,,7
L27:
	xorl %eax,%eax
L30:
	movl -20(%ebp),%ebx
	movl %ebp,%esp
	popl %ebp
	ret
	.align 4
.globl _pciWrite
	.def	_pciWrite;	.scl	2;	.type	32;	.endef
_pciWrite:
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	pushl %edi
	orl $-2147483648,%ecx
	xorl %eax,%eax
	pushl %esi
	andl $-2147418113,%ecx
	xorl %edx,%edx
	pushl %ebx
	movl 24(%ebp),%esi
	movl 28(%ebp),%edi
	movl 20(%ebp),%ebx
	movb 8(%ebp),%al
	sall $16,%eax
	orl %eax,%ecx
	movb 12(%ebp),%dl
	andl $31,%edx
	sall $11,%edx
	andb $7,%ch
	orl %edx,%ecx
	xorl %eax,%eax
	movb 16(%ebp),%al
	andl $7,%eax
	sall $8,%eax
	andb $248,%ch
	orl %eax,%ecx
	movb 20(%ebp),%dl
	andb $-4,%dl
	movb %dl,-4(%ebp)
	movb %dl,%cl
	andl $3,%ebx
	addl $3324,%ebx
	movl $3320,%edx
	movl %ecx,%eax
/APP
	outl	%eax, %dx
/NO_APP
	cmpl $2,%edi
	je L35
	jg L41
	cmpl $1,%edi
	je L34
	jmp L33
	.p2align 4,,7
L41:
	cmpl $4,%edi
	je L37
	jmp L33
	.p2align 4,,7
L34:
	movl %ebx,%edx
	movl %esi,%eax
/APP
	outb	%al, %dx
/NO_APP
	jmp L33
	.p2align 4,,7
L35:
	movl %ebx,%edx
	movl %esi,%eax
/APP
	outw	%ax, %dx
/NO_APP
	jmp L33
	.p2align 4,,7
L37:
	movl %ebx,%edx
	movl %esi,%eax
/APP
	outl	%eax, %dx
/NO_APP
L33:
	leal -28(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
	.align 32
LC0:
	.ascii "D\0e\0v\0i\0c\0e\0 \0I\0n\0f\0o\0:\0 \0/\0b\0u\0s\0/\0p\0c\0i\0/\0%\0d\0/\0%\0d\0/\0%\0d\0\12\0\0\0"
	.align 32
LC1:
	.ascii " \0 \0*\0 \0V\0e\0n\0d\0o\0r\0:\0 \0%\0X\0 \0 \0 \0D\0e\0v\0i\0c\0e\0:\0 \0%\0X\0 \0 \0C\0l\0a\0s\0s\0/\0S\0u\0b\0C\0l\0a\0s\0s\0/\0I\0n\0t\0e\0r\0f\0a\0c\0e\0 \0%\0X\0/\0%\0X\0/\0%\0X\0\12\0\0\0"
	.align 32
LC2:
	.ascii " \0 \0*\0 \0S\0t\0a\0t\0u\0s\0:\0 \0%\0X\0 \0 \0C\0o\0m\0m\0a\0n\0d\0:\0 \0%\0X\0 \0 \0B\0I\0S\0T\0/\0T\0y\0p\0e\0/\0L\0a\0t\0/\0C\0L\0S\0:\0 \0%\0X\0/\0%\0X\0/\0%\0X\0/\0%\0X\0\12\0\0\0"
	.align 32
LC3:
	.ascii " \0 \0*\0 \0I\0n\0t\0e\0r\0r\0u\0p\0t\0 \0L\0i\0n\0e\0:\0 \0%\0X\0\12\0\0\0"
	.align 32
LC4:
	.ascii " \0 \0*\0 \0P\0C\0I\0 \0<\0-\0>\0 \0P\0C\0I\0 \0B\0r\0i\0d\0g\0e\0\12\0\0\0"
	.align 32
LC5:
	.ascii " \0 \0*\0 \0U\0n\0k\0n\0o\0w\0n\0 \0H\0e\0a\0d\0e\0r\0 \0T\0y\0p\0e\0\12\0\0\0"
	.align 4
.globl _pciProbe
	.def	_pciProbe;	.scl	2;	.type	32;	.endef
_pciProbe:
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl $0,-4(%ebp)
	xorl %ebx,%ebx
	.p2align 4,,7
L46:
	pushl $4
	pushl %ebx
	movl 16(%ebp),%edx
	pushl %edx
	movl 12(%ebp),%ecx
	pushl %ecx
	movl 8(%ebp),%edx
	pushl %edx
	call _pciRead
	movl 20(%ebp),%ecx
	movl %eax,(%ecx,%ebx)
	addl $20,%esp
	addl $4,%ebx
	incl -4(%ebp)
	cmpl $3,-4(%ebp)
	jle L46
	cmpw $65535,(%ecx)
	jne L48
	movl $1,%eax
	jmp L65
	.p2align 4,,7
L48:
	movl 20(%ebp),%ecx
	movb 8(%ebp),%dl
	movb %dl,16(%ecx)
	movb 12(%ebp),%dl
	movb %dl,17(%ecx)
	movb 16(%ebp),%dl
	movb %dl,18(%ecx)
	movl 16(%ebp),%ecx
	pushl %ecx
	movl 12(%ebp),%edx
	pushl %edx
	movl 8(%ebp),%ecx
	pushl %ecx
	pushl $LC0
	call _wprintf
	movl 20(%ebp),%edx
	xorl %eax,%eax
	movb 9(%edx),%al
	pushl %eax
	xorl %eax,%eax
	movb 10(%edx),%al
	pushl %eax
	xorl %eax,%eax
	movb 11(%edx),%al
	pushl %eax
	xorl %eax,%eax
	movw 2(%edx),%ax
	pushl %eax
	xorl %eax,%eax
	movw (%edx),%ax
	pushl %eax
	pushl $LC1
	call _wprintf
	movl 20(%ebp),%ecx
	addl $40,%esp
	xorl %eax,%eax
	movb 12(%ecx),%al
	pushl %eax
	xorl %eax,%eax
	movb 13(%ecx),%al
	pushl %eax
	xorl %eax,%eax
	movb 14(%ecx),%al
	pushl %eax
	xorl %eax,%eax
	movb 15(%ecx),%al
	pushl %eax
	xorl %eax,%eax
	movw 4(%ecx),%ax
	pushl %eax
	xorl %eax,%eax
	movw 6(%ecx),%ax
	pushl %eax
	pushl $LC2
	call _wprintf
	movl 20(%ebp),%edx
	addl $28,%esp
	movb 14(%edx),%al
	andb $127,%al
	andl $255,%eax
	testl %eax,%eax
	je L50
	cmpl $1,%eax
	je L62
	jmp L63
	.p2align 4,,7
L50:
	movl 20(%ebp),%ecx
	movl 20(%ebp),%edx
	movl $0,-4(%ebp)
	addl $44,%ecx
	movl %ecx,-8(%ebp)
	addl $20,%edx
	movl %edx,-12(%ebp)
	movl $16,%edi
	.p2align 4,,7
L54:
	pushl $4
	pushl %edi
	movl 16(%ebp),%ecx
	pushl %ecx
	movl 12(%ebp),%edx
	pushl %edx
	movl 8(%ebp),%ecx
	pushl %ecx
	call _pciRead
	movl %eax,%esi
	addl $20,%esp
	testl %esi,%esi
	je L55
	pushl $4
	pushl $-1
	pushl %edi
	movl 16(%ebp),%edx
	pushl %edx
	movl 12(%ebp),%ecx
	pushl %ecx
	movl 8(%ebp),%edx
	pushl %edx
	call _pciWrite
	pushl $4
	pushl %edi
	movl 16(%ebp),%ecx
	pushl %ecx
	movl 12(%ebp),%edx
	pushl %edx
	movl 8(%ebp),%ecx
	pushl %ecx
	call _pciRead
	addl $44,%esp
	pushl $4
	pushl %esi
	pushl %edi
	movl 16(%ebp),%edx
	pushl %edx
	movl %eax,%ebx
	movl 12(%ebp),%ecx
	pushl %ecx
	andl $-16,%ebx
	movl 8(%ebp),%edx
	pushl %edx
	call _pciWrite
	movl %ebx,%eax
	notl %eax
	leal 1(%eax),%ebx
	addl $24,%esp
	testl $1,%esi
	je L56
	movl -12(%ebp),%ecx
	movl -8(%ebp),%edx
	andl $65535,%esi
	movl %esi,(%ecx)
	andl $65535,%ebx
	movl %ebx,(%edx)
	jmp L53
	.p2align 4,,7
L56:
	movl -12(%ebp),%ecx
	movl -8(%ebp),%edx
	movl %esi,(%ecx)
	movl %ebx,(%edx)
	jmp L53
	.p2align 4,,7
L55:
	movl -12(%ebp),%ecx
	movl -8(%ebp),%edx
	movl $0,(%ecx)
	movl $0,(%edx)
L53:
	addl $4,-8(%ebp)
	addl $4,-12(%ebp)
	addl $4,%edi
	incl -4(%ebp)
	cmpl $5,-4(%ebp)
	jle L54
	pushl $1
	pushl $60
	movl 16(%ebp),%ecx
	pushl %ecx
	movl 12(%ebp),%edx
	pushl %edx
	movl 8(%ebp),%ecx
	pushl %ecx
	call _pciRead
	movl %eax,%esi
	addl $20,%esp
	cmpl $255,%esi
	je L60
	movl %esi,%edx
	movl 20(%ebp),%ecx
	movb %dl,19(%ecx)
	jmp L61
	.p2align 4,,7
L60:
	movl 20(%ebp),%edx
	movb $0,19(%edx)
L61:
	movl 20(%ebp),%ecx
	xorl %eax,%eax
	movb 19(%ecx),%al
	pushl %eax
	pushl $LC3
	call _wprintf
	jmp L49
	.p2align 4,,7
L62:
	pushl $LC4
	jmp L66
	.p2align 4,,7
L63:
	pushl $LC5
L66:
	call _wprintf
L49:
	xorl %eax,%eax
L65:
	leal -28(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
	.align 4
.globl _pciFind
	.def	_pciFind;	.scl	2;	.type	32;	.endef
_pciFind:
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	pushl %edi
	pushl %esi
	xorl %esi,%esi
	pushl %ebx
	movl 16(%ebp),%edi
	movl 8(%ebp),%edx
	movw %dx,-2(%ebp)
	movl 12(%ebp),%edx
	movw %dx,-4(%ebp)
	.p2align 4,,7
L71:
	xorl %ebx,%ebx
	xorl %edx,%edx
	movw %si,%dx
	movl %edx,-8(%ebp)
	.p2align 4,,7
L75:
	pushl %edi
	pushl $0
	xorl %eax,%eax
	movw %bx,%ax
	pushl %eax
	movl -8(%ebp),%edx
	pushl %edx
	call _pciProbe
	addl $16,%esp
	testl %eax,%eax
	jne L74
	movw -2(%ebp),%dx
	cmpw %dx,(%edi)
	jne L74
	movl -4(%ebp),%edx
	cmpw %dx,2(%edi)
	jne L74
	movl $1,%eax
	jmp L80
	.p2align 4,,7
L74:
	incw %bx
	cmpw $31,%bx
	jbe L75
	incw %si
	cmpw $254,%si
	jbe L71
	xorl %eax,%eax
L80:
	leal -28(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
.section	.init,"x"
	.align 4
.globl _drvInit@12
	.def	_drvInit@12;	.scl	2;	.type	32;	.endef
_drvInit@12:
	pushl %ebp
	movl %esp,%ebp
	movl $1,%eax
	movl %ebp,%esp
	popl %ebp
	ret $12
	.def	_wprintf;	.scl	2;	.type	32;	.endef
