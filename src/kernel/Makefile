OBJS=		start.o isr.o kstart.o i386.o \
	main.o memory.o vm86.o thread.o \
	module.o mod_pe.o device.o \
	syscall.o ramdisk.o handle.o \
	vmm.o serial.o obj.o sys.o \
	fs.o config.o cache.o port.o \
	font8x8.o font8x16.o \
	mbstowcs.o wcstombs.o strtok.o wprintf.o \
	hash.o \
	iswupper.o iswlower.o towupper.o towlower.o

#	memset.o memcpy.o 
#	unicode.o 
#	wcslen.o wcsicmp.o wcsncpy.o wcscpy.o wcsrchr.o wcscat.o wcscmp.o wcsdup.o 

CPPFLAGS=	-DKERNEL -ffreestanding
LDFLAGS=	--file-alignment 0x1000 --image-base 0xc0000000 --subsystem native \
	-T kernel.ld --entry _NtProcessStartup
LIBS=		$(LIB)/libcs.lib

include ../make.actions

TARGET=		$(BIN)/kernel.exe

all:	$(TARGET) $(RAMDISK) $(LIB)\kernel.lib

kernel.exp: kernel.def
	dlltool -e kernel.exp -m i386 --input-def kernel.def -D kernel.exe

$(LIB)\kernel.lib: kernel.def
#	dlltool -xc -m i386 --input-def kernel.def --output-lib $(LIB)\kernel.lib
	lib /def:kernel.def /out:$(LIB)\kernel.lib /nologo /machine:ix86

$(TARGET): Makefile $(OBJS) $(LIBS) kernel.ld kernel.exp
	ld -g -o $(TARGET) \
		$(OBJS) kernel.exp \
		$(LIBS) $(LDFLAGS)
	objdump -ld $(TARGET) > listing.txt
	nm $(TARGET) | sort > symbols.txt
#	ld -s -o $(TARGET) \
#		$(OBJS) kernel.exp \
#		$(LIBS) $(LDFLAGS)

clean:
	rm $(OBJS)
	rm $(TARGET)
	rm $(OBJS:.o=.d)
	rm listing.txt
	rm symbols.txt

rebuild: clean all

include $(OBJS:.o=.d)