OBJS=	start.o isr.o kstart.o i386.o \
	main.o memory.o vm86.o thread.o \
	module.o mod_pe.o device.o \
	syscall.o ramdisk.o handle.o \
	vmm.o serial.o obj.o sys.o \
	fs.o config.o cache.o port.o \
	vga.o text.o vga4.o \
	../kernelu/fullpath.o ../kernelu/resource.o \
	font8x8.o font8x16.o cupertino.o t.o \
	mbstowcs.o wcstombs.o strtok.o wprintf.o \
	hash.o
RES=	kernel.res.o

CFLAGS=	-DKERNEL -ffreestanding -g
LDFLAGS=--section-alignment 0x1000 --file-alignment 0x1000 \
	--image-base 0xc0000000 --subsystem native \
	-T kernel.ld --entry _keEntry
LIBS=		$(LIB)/libc_s.lib

include	../make.actions

TARGET=	$(BIN)/kernel.exe

all:	$(TARGET) $(RAMDISK) $(LIB)/kernel.lib

vga4.d:	../drivers/video/vga4.c
	$(CC) $(CFLAGS) -M -c $< -o $@

vga4.o:	../drivers/video/vga4.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.res.o:	kernel.rc logo2.bmp

kernel.exp:	kernel.def
	dlltool -e kernel.exp -m i386 --input-def kernel.def -D kernel.exe

$(LIB)/kernel.lib:	kernel.def
	dlltool -m i386 --input-def kernel.def --output-lib $(LIB)/kernel.lib

$(TARGET):	Makefile $(OBJS) $(LIBS) $(RES) kernel.ld kernel.exp
	ld -s -o $(TARGET) \
		$(OBJS) \
		kernel.exp \
		$(LIBS) $(RES) $(LDFLAGS)
#	objdump -ld $(TARGET) > listing.txt
#	nm $(TARGET) | sort > symbols.txt

clean:
	rm $(OBJS)
	rm $(TARGET)
	rm $(OBJS:.o=.d)
	rm listing.txt
	rm symbols.txt

rebuild:	clean all

include $(OBJS:.o=.d)