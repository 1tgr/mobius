BOOT1=	kernel.exe
BOOT1=	ata.drv ps2mouse.drv 
BOOT2=	fat.drv tty.drv keyboard.drv fdc.drv \
	cmos.drv video.drv pci.drv 
BOOT3=	shell.exe libc.dll libsys.dll 
BOOT4=	test.exe test.txt kernel.raw
BOOT5=	lucsdm.ttf sb.kll sound.drv ne2000.drv
GRUB=	menu.lst 
OS1=	folder.wmf banner.wmf ../src/shell/v86.asm ../src/kernel/start.asm \
	vbe.exe mgltest.exe vidtest.exe nasm.exe \
	ndisasm.exe v86.com hello.exe 
ifeq ($(DEST),)
DEST=	c:\\
endif

MCOPY=	./mcopy

all:
	$(MCOPY) -o $(GRUB) $(DEST)
	$(MCOPY) -o system.pro.bochs $(DEST)boot\\system.pro
	$(MCOPY) -o kernel.exe $(DEST)boot
	$(MCOPY) -o $(BOOT1) $(DEST)boot
	$(MCOPY) -o $(BOOT2) $(DEST)boot
	$(MCOPY) -o $(BOOT3) $(DEST)boot
	$(MCOPY) -o $(BOOT4) $(DEST)boot
	$(MCOPY) -o $(BOOT5) $(DEST)boot
	$(MCOPY) -o $(OS1) $(DEST)mobius

#floppy:
#	for f in $(FILES1) $(FILES2); do \
#		echo $$f ;\
#		cp $$f /cygdrive/a/; \
#		done


floppy:
	raw32
