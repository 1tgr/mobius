TARGET_D=	$(BIN)/gui.dll
TARGET_S=	$(LIB)/gui_s.lib
OBJS=	dllmain.o window.o button.o frame.o \
	application.o view.o dialog.o edit.o \
	label.o control.o container.o messagebox.o
DEF=	gui.def
IMP=	$(LIB)/gui.lib
LIBS=	$(LIB)/mgl.lib $(LIB)/libc.lib $(LIB)/libsys.lib
BASE=	gui

include ../make.conf
include ../make.actions

all:
	make -C desktop
	make -C test
