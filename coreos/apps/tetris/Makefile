ROOT=	../../..

TARGET=	$(BIN)/tetris.exe
OBJS=	tetris.o
LIBS=	$(LIB)/crt0.o $(LIB)/framebuf_user.lib $(LIB)/freetype.lib $(LIB)/libc.lib $(LIB)/libsys.lib
CFLAGS=	-I$(ROOT)/experimental/freetype-2.1.5/include

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions
