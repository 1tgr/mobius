BOOT1=	kernel.exe ata.drv ps2mouse.drv system.pro
BOOT2=	fat.drv tty.drv keyboard.drv fdc.drv \
	cmos.drv video.drv pci.drv 
BOOT3=	shell.exe hello.exe libc.dll v86.com \
	ndisasm.exe nasm.exe libsys.dll 
BOOT4=	vidtest.exe mgl.dll mgltest.exe tetris.exe \
	test.exe test.txt vbe.exe
GRUB=	menu.lst 
OS1=	folder.wmf banner.wmf ..\\src\\shell\\v86.asm ..\\src\\kernel\\start.asm 
ifeq ($(DEST),)
DEST=	c:\\
endif

all:
	mcopy.cmd -o $(GRUB) $(DEST)
	mcopy.cmd -o $(BOOT1) $(DEST)boot
	mcopy.cmd -o $(BOOT2) $(DEST)boot
	mcopy.cmd -o $(BOOT3) $(DEST)boot
	mcopy.cmd -o $(BOOT4) $(DEST)boot
	mcopy.cmd -o $(OS1) $(DEST)mobius

#floppy:
#	for f in $(FILES1) $(FILES2); do \
#		echo $$f ;\
#		cp $$f /cygdrive/a/; \
#		done


floppy:
	raw32
