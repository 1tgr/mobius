ROOT=	../../..

TARGET=	$(BIN)/wincli.exe
OBJS=	client.o
LIBS=	$(LIB)/crt0.o $(LIB)/gui.lib $(LIB)/libc.lib $(LIB)/libsys.lib
CFLAGS=	-O2 -I../include -I$(ROOT)/include/stlport

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions
