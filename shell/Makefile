TARGET=	$(BIN)/shell.exe
OBJS=	shell.o line.o params.o v86.o shell.res.o
LIBS=	$(LIB)/crt0.o $(LIB)/libc.lib $(LIB)/libsys.lib

include ../make.conf
include ../make.actions

all: $(TARGET) $(BIN)/v86.com

$(BIN)/v86.com: v86.asm
	nasm -f bin $< -o `cygpath -w $@`

%.com: %.asm
	nasm -f bin $< -o `cygpath -w $@`

shell.res.o: shell.rc
