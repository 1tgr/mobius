TARGET=	$(BIN)/vbe.exe
OBJS=	vbe.o vbe.res.o
LIBS=	$(LIB)/crt0.o $(LIB)/libc.lib $(LIB)/libsys.lib

include ../make.conf
include ../make.actions

all:	$(TARGET)

vbe.res.o:  vbe.rc biosvid.com

%.com:	%.asm
	nasm -f bin $< -o `cygpath -w $@`
