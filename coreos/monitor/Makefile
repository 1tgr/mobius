ROOT=	../..
OBJS=	monitor.o devices.o malsmall.o \
    ../libc/string/memset.o ../libc/string/wcsicmp.o ../libc/stdlib/qsort.o
TARGET=	$(BIN)/monitor.exe
LIBS=	$(LIB)/framebuf_user.lib $(LIB)/libsys_s.lib

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions
