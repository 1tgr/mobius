TARGET=	$(BIN)/kernel.exe
ARCH=	i386/libi386.lib
OBJS=	start.o main.o thread.o \
	proc.o handle.o fs.o \
	device.o io.o vmm.o \
	mod_pe.o syscall.o port.o \
	ramdisk_mb.o cache.o cppsup.o \
	rtlsup.o debug.o profile.o
LIBS=	$(ARCH) $(LIB)/librtl_s.lib $(LIB)/libc_s.lib
DEFINES=-Di386 -DKERNEL
DEF=	kernel.def
IMP=	$(LIB)/kernel.lib
EXP=	kernel.exp
LDFLAGS=	--image-base 0xC0000000 --subsystem native \
	--file-alignment 0x1000 --section-alignment 0x1000
CPPFLAGS+=	-fvtable-thunks

include ../make.conf

all:	$(ARCH) $(TARGET) $(IMP)

$(ARCH):
	make -C i386

$(IMP):	$(DEF)
	dlltool -d $< -l $@ -D $(<:.def=.exe)

$(TARGET):	$(ARCH) $(OBJS) $(LIBS) $(EXP)
	$(LD) -o kernel_d.exe $(OBJS) $(EXP) $(LIBS) \
		$(LDFLAGS) --entry _KernelEntry -T kernel.ld
	objdump -ld kernel_d.exe > listing.txt
	$(LD) -o $(TARGET) $(OBJS) $(EXP) $(LIBS) \
		$(LDFLAGS) --strip-all --entry _KernelEntry -T kernel.ld

clean:
	make -C i386 clean
	rm $(OBJS:.o=.d)
	rm $(OBJS)
	rm $(TARGET)
	rm $(EXP)
	rm $(IMP)

include $(OBJS:.o=.d)
