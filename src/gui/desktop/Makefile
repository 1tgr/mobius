TARGET=	$(BIN)/desktop.exe
OBJS=	desktop.o alttabwindow.o desktopapplication.o \
	v86.o desktop.res.o ../../crt0.o
LIBS=	$(LIB)/gui.lib $(LIB)/libc.lib $(LIB)/libsys.lib $(LIB)/mgl.lib

include ../../make.conf
include ../../make.actions

desktop.res.o: poweroff.com

%.com: %.asm
	nasm -f bin $< -o `cygpath -w $@`
