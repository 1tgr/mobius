FILES1=	test.exe test.txt libsys.dll \
	kernel.exe kernel.cfg ata.drv 
FILES2=	fat.drv tty.drv keyboard.drv fdc.drv \
	cmos.drv video.drv shell.exe 
FILES3=	hello.exe libc.dll v86.com ndisasm.exe \
	nasm.exe ..\\src\\kernel\\start.asm ..\\src\\shell\\v86.asm
FILES4=	vidtest.exe mgl.dll mgltest.exe
GRUB=	menu.lst 
ifeq ($(DEST),)
DEST=	c:\\
endif

all:
	mcopy.cmd -o $(GRUB) $(DEST)
	mcopy.cmd -o $(FILES1) $(DEST)boot
	mcopy.cmd -o $(FILES2) $(DEST)boot
	mcopy.cmd -o $(FILES3) $(DEST)boot
	mcopy.cmd -o $(FILES4) $(DEST)boot

#floppy:
#	for f in $(FILES1) $(FILES2); do \
#		echo $$f ;\
#		cp $$f /cygdrive/a/; \
#		done


floppy:
	raw32
