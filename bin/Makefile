LOADER=	bootsect.com
LOADER2=mobel_pe.com
RAMDISK=ramdisk.bin
KERNEL=	kernel.exe

SYS=	isa.cfg drivers.cfg pci.drv isa.drv \
	ps2mouse.drv sermouse.drv keyboard.drv ata.drv \
	fdc.drv fat.drv vgatext.drv cmos.drv \
	kdebug.dll devtest.exe kernelu.dll libc.dll \
	tetris.exe short.exe console.exe hello.exe
APPS=	ls.exe mount.exe
DEMOS=	tetris.exe hello.exe guiapp.exe sertest.exe
MISC=	hello.txt
FORTUNE=fortune.exe fortunes
FILES=	$(SYS)

include ../src/make.actions

all:	$(RAMDISK) $(LOADER) $(LOADER2)
	mdel.cmd a:*.*
	mcopy.cmd -o $(LOADER2) a:
	mcopy.cmd -o $(KERNEL) a:
	mcopy.cmd -o $(RAMDISK) a:

floppy:	all
#	dd if=$(LOADER) of=a:
	format a: /q < format.txt
	rawrite -f $(LOADER) -d a: -n
	copy $(LOADER2) a:
	copy $(KERNEL) a:
	copy $(RAMDISK) a:

#$(LOADER):	$(RAMDISK) $(KERNEL)

$(RAMDISK):	$(FILES) $(KERNEL) Makefile
	$(BIN)/ramdisk -o $(RAMDISK) $(FILES)

clean:
	rm $(RAMDISK)