LOADER=	bootsect.com
LOADER2=mobel_pe.com
RAMDISK=ramdisk.bin
KERNEL=	kernel.exe

SYS=	kernel.cfg isa.cfg drivers.cfg
#ata.drv sermouse.drv vgatext.drv cmos.drv sound.drv
FILES=	$(SYS)

SYSUSER=kernelu.dll libc.dll mgl.dll mgltest.exe \
	freetype.dll rezn000.ttf
APPS=	ls.exe mount.exe fttest.exe
DEMOS=	tetris.exe hello.exe guiapp.exe sertest.exe
MISC=	coffbase.txt
FORTUNE=fortune.exe fortunes

DISKFILES=	$(LOADER2) $(KERNEL) $(FILES) $(SYS)
HDFILES=	

include ../src/make.actions

all:	$(RAMDISK) $(LOADER) $(LOADER2)
#	mcopy.cmd -o $(DISKFILES) a:
	for %i in ($(DISKFILES)) do @mcopy.cmd -o %i a:
	for %i in ($(HDFILES)) do @mcopy.cmd -o %i c:

floppy:	all
#	dd if=$(LOADER) of=a:
	format a: /q < format.txt
	rawrite -f $(LOADER) -d a: -n
	cp $(DISKFILES) a:

#$(LOADER):	$(RAMDISK) $(KERNEL)

#$(RAMDISK):	$(FILES) $(KERNEL) Makefile
#	$(BIN)/ramdisk -o $(RAMDISK) $(FILES)

clean:
	rm $(RAMDISK)