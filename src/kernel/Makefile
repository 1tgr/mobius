TARGET=	$(BIN)/kernel.exe

# To build the SMP version, uncomment this line and comment out the other one
#DEFINES=-Di386 -DKERNEL -D__LIBC_STATIC -D__SMP__

# To build the UP version, uncomment this line and comment out the other one
DEFINES=-Di386 -DKERNEL -D__LIBC_STATIC

# Uncomment this and do 'make clean all' for a text-mode kernel
DEFINES+=	-DMODE_TEXT

ARCH=	i386/isr.o i386/i386.o i386/memory.o i386/arch.o \
	i386/scdsptch.o i386/gdb-stub.o i386/gdb.o i386/poweroff.o \
	../drivers/video/font8x8.o

ifeq (,$(findstring __SMP__,$(DEFINES)))
ARCH+=	i386/uniproc.o
else
ARCH+=  i386/smp.o i386/bsp.o
endif

ifeq (,$(findstring MODE_TEXT,$(DEFINES)))
ARCH+=	../drivers/video/vga.o ../drivers/video/vga4.o \
	../drivers/video/vgamodes.o
endif

OBJS=	start.o main.o thread.o \
	proc.o handle.o fs.o vfs.o \
	device.o io.o vmm.o \
	mod_pe.o syscall.o \
	ramdisk_mb.o cache.o cppsup.o \
	rtlsup.o debug.o profile.o winmgr.o \
	queue.o clip.o kernel.res.o \
	$(ARCH)
LIBS=	$(LIB)/librtl_s.lib $(LIB)/libc_s.lib
DEF=	kernel.def
IMP=	$(LIB)/kernel.lib
EXP=	kernel.exp
LDFLAGS=	--image-base 0xC0000000 --subsystem native \
	--file-alignment 0x1000 --section-alignment 0x1000
CPPFLAGS+=	-fvtable-thunks

include ../make.conf

all:	$(TARGET) $(IMP)

$(IMP):	$(DEF)
	dlltool -d $< -l $@ -D $(<:.def=.exe)

$(TARGET):	$(OBJS) $(LIBS) $(EXP)
	$(LD) -o kernel_d.exe $(OBJS) $(EXP) $(LIBS) \
		$(LDFLAGS) --entry _KernelEntry -T kernel.ld
	objdump -ld kernel_d.exe > listing.txt
	$(LD) -o $(TARGET) $(OBJS) $(EXP) $(LIBS) \
		$(LDFLAGS) --strip-all --entry _KernelEntry -T kernel.ld

clean:
	rm $(OBJS:.o=.d)
	rm $(OBJS)
	rm $(TARGET)
	rm $(EXP)
	rm $(IMP)

include $(OBJS:.o=.d)
