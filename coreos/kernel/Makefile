ROOT=	../..
TARGET=	$(BIN)/kernel.exe

# To build the SMP version, uncomment this line and comment out the other one
#DEFINES=-Di386 -DKERNEL -D__LIBC_STATIC -D__SMP__

# To build the UP version, uncomment this line and comment out the other one
DEFINES=-Di386 -DKERNEL -D__LIBC_STATIC

# Uncomment this and do 'make clean all' for a text-mode kernel
DEFINES+=	-DMODE_TEXT

ARCH=	i386/start.o i386/isr.o i386/spinlock.o i386/i386.o \
	i386/memory.o i386/arch.o i386/scdsptch.o i386/gdb.o \
	i386/poweroff.o font8x8.o

ifeq (,$(findstring __SMP__,$(DEFINES)))
ARCH+=	i386/uniproc.o
else
ARCH+=  i386/smp.o i386/bsp.o
endif

ifeq (,$(findstring MODE_TEXT,$(DEFINES)))
ARCH+=	../drivers/video/vga4.o ../drivers/video/vga.o  \
	../drivers/video/vgamodes.o
endif

LIBRTL=	../libc/string/wcscpy.o ../libc/string/wcsncpy.o ../libc/string/wcslen.o ../libc/string/wcscat.o \
	../libc/string/wcscmp.o ../libc/string/wcsicmp.o ../libc/string/wcsncmp.o ../libc/string/wcsrchr.o \
	../libc/string/wcschr.o ../libc/string/wcsnicmp.o \
	../libc/string/strcpy.o ../libc/string/strncpy.o ../libc/string/strlen.o ../libc/string/strcat.o \
	../libc/string/strcmp.o ../libc/string/stricmp.o ../libc/string/strncmp.o ../libc/string/strrchr.o \
	../libc/string/strchr.o \
	../libc/string/djmd.o ../libc/string/djmdr.o ../libc/string/memcpy.o ../libc/string/memmove.o \
	../libc/string/memset.o ../libc/string/memcmp.o \
	../libc/string/mbstowcs.o ../libc/string/wcstombs.o \
	../libc/stdlib/malloc.o ../libc/stdlib/malwrap.o ../libc/stdlib/wcstol.o ../libc/stdlib/wcstoul.o \
	../libc/stdio/dowprintf.o \
	../libc/ctype/tolower.o ../libc/ctype/towlower.o \
	../libc/ctype/islower.o ../libc/ctype/iswlower.o \
	../libc/ctype/toupper.o ../libc/ctype/towupper.o \
	../libc/ctype/isupper.o ../libc/ctype/iswupper.o \
	../libc/ctype/ct_lower.o ../libc/ctype/ct_upper.o ../libc/ctype/ct_flags.o \
	../libc/sys/int64.o \
	../librtl/fullpath.o

OBJS=	$(ARCH) \
	main.o thread.o proc.o handle.o \
	fs.o file.o create_open.o read_write.o \
	directory.o vfs.o device.o dfs.o \
	isa.o io.o profile.o swprintf.o \
	vmm.o pipe.o mod_pe.o syscall.o \
	port.o ramdisk_mb.o cache.o cppsup.o \
	rtlsup.o maldebug.o debug.o wprintf.o \
	counter.o ramdisk_tar.o \
	queue.o slab.o bitmap.o \
	kernel.res.o
LIBS=	$(OBJDIR)/librtl.a
DEF=	kernel.def
IMP=	$(LIB)/kernel.lib
EXP=	$(OBJDIR)/kernel.exp
LDFLAGS=	--image-base 0xC0000000 --subsystem native \
	--file-alignment 0x1000 --section-alignment 0x1000

include $(ROOT)/coreos/make.conf

all:	$(TARGET) $(TARGET).gz $(IMP)

LIBRTL_IN_DIR=	$(addprefix $(OBJDIR)/, $(LIBRTL))

include $(OBJS_IN_DIR:.o=.d)
include $(LIBRTL_IN_DIR:.o=.d)

$(OBJDIR)/librtl.a:	$(LIBRTL_IN_DIR)
	ar rcs $@ $(LIBRTL_IN_DIR)

$(IMP):	$(DEF)
	dlltool -d $< -l $@ -D $(<:.def=.exe)

$(TARGET):	$(OBJS_IN_DIR) $(LIBS) $(EXP)
	$(LD) -o $(BIN)/kernel_d.exe $(OBJS_IN_DIR) $(EXP) $(LIBS) \
		$(LDFLAGS) -Map kernel.map --entry _KernelEntry -T kernel.ld
	$(LD) -o $(TARGET) $(OBJS_IN_DIR) $(EXP) $(LIBS) \
		$(LDFLAGS) --strip-all --entry _KernelEntry -T kernel.ld

$(TARGET).gz:	$(TARGET)
	gzip -f $(TARGET)

clean:
	rm $(OBJS_IN_DIR:.o=.d)
	rm $(OBJS_IN_DIR)
	rm $(addprefix $(OBJDIR)/, $(LIBRTL))
	rm $(TARGET).gz
	rm $(EXP)
	rm $(IMP)
