ROOT=	../../..

TARGET=	$(BIN)/shell.exe
OBJS=	cd.o colour.o device.o dir.o \
    dismount.o exec.o help.o info.o \
    line.o mount.o params.o pipe.o \
    poke.o shell.o test.o typedump.o \
    v86.o shell.res.o
LIBS=	$(LIB)/crt0.o $(LIB)/libc.lib $(LIB)/libsys.lib

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions

all: $(TARGET) $(BIN)/v86.com

$(BIN)/v86.com: v86.asm
	nasm -f bin $< -o `cygpath -w $@`

%.oom: %.asm
	nasm -f bin $< -o `cygpath -w $@`

shell.res.o: shell.rc
