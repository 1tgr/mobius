ROOT=	../..
OBJDIR=	objects

#EXP=    gui.exp
#DEF=    gui.def

OBJS=	dllmain.o gfxstubs.o ipc.o stubs.o \
	Button.o Control.o DialogBox.o DialogLayout.o \
	Edit.o FrameWindow.o Graphics.o Label.o \
	Menu.o WindowImpl.o

BASE=	gui
CFLAGS=		-DLIBGUI_EXPORTS
LDFLAGS=-shared --out-implib=$(LIB)/gui.lib

include $(ROOT)/coreos/make.conf

TARGET_D=       $(BIN)/gui.dll
TARGET_S=       $(LIB)/gui_s.lib
IMP=            $(LIB)/gui.lib
LIBS=           $(LIB)/libc.lib $(LIB)/libsys.lib

include $(ROOT)/coreos/make.actions
