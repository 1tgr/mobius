EXP=	posix.exp
DEF=	posix.def
#DEFINES=-D__STRICT_ANSI__
#DEFINES=-D__LIBC_STATIC
DEFINES=-D_POSIX_SOURCE -D__LIBPOSIX_STATIC
BASE=	posix
CFLAGS=	-I$(INCLUDE)/posix

include ../make.conf

TARGET_D=	$(BIN)/posix.dll
TARGET_S=	$(LIB)/libposix_s.lib
IMP=		$(LIB)/libposix.lib
LIBS=		regex/libregex.lib $(LIB)/libc_s.lib $(LIB)/libsys.lib

OBJS=	access.o    dllmain.o  getopt.o getpid.o \
	htons.o   ntohl.o  read.o  wopen.o \
	close.o     isatty.o  ntohs.o  sbrk.o  write.o \
	creatnew.o  htonl.o    lseek.o   open.o   opendir.o \
	random.o readdir.o srandom.o sleep.o stat.o

include ../make.actions

regex/libregex.lib:
	make -C regex
