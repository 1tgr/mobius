ROOT=	../../../..
TARGET_D=	$(BIN)/vga.kll
OBJS=	vga.o vga4.o vga8.o vgamodes.o
BASE=	vga
LIBS=	$(LIB)/kernel.lib $(LIB)/video.lib
DEFINES=-DKERNEL -I../include
CFLAGS=	-O2

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions
