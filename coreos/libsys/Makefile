ROOT=	../..

EXP=    libsys.exp
DEF=    libsys.def

#
# This is ugly -- libsys depends on certain libc functions. The alternatives 
#   to this (direcly including the relevant libc .o's) are:
# * Copying & pasting the libc code into libsys
# * Copying the sources from libc into libsys
# * Linking to libc_s.lib (OK, but building from scratch is a pain)
# Anyway, if you start using any more libc functions, and the linker complains,
#   put the relevant object file names here.
# Also, "make clean" in /src breaks because both libc and libsys try to clean
#   these .o's. Ah, well. Make sure you do "make clean -i" to be safe.
#
OBJS=	dllmain.o syscall.o port.o misc.o \
	resource.o fs.o queue.o map437.o \
	debug.o lmutex.o spinlock.o startup.o \
	../libc/errno/errno.o ../libc/string/wcslen.o \
	../libc/string/memcpy.o ../libc/string/wcschr.o \
	../libc/string/wcscat.o ../libc/string/wcscmp.o \
	../libc/string/wcscpy.o ../libc/string/wcsncpy.o \
	../libc/string/strncmp.o ../libc/string/strlen.o \
	../libc/string/mbstowcs.o ../libc/stdio/dowprintf.o \
	../librtl/fullpath.o
BASE=	libsys

include $(ROOT)/coreos/make.conf

TARGET_D=       $(BIN)/libsys.dll
TARGET_S=       $(LIB)/libsys_s.lib
IMP=            $(LIB)/libsys.lib
LIBS=           

include $(ROOT)/coreos/make.actions

sysdef.i:	sysdef.c
	gcc -E $< -o $@ -I$(INCLUDE)
