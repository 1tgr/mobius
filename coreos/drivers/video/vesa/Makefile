ROOT=	../../../..
TARGET_D=	$(BIN)/vesa.kll
OBJS=	vesa.o
BASE=	vesa
LIBS=	$(LIB)/kernel.lib $(LIB)/video.lib
DEFINES=-DKERNEL -I../include

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions

biosvid.h:	biosvid.com
	./boot2h $< > $@

biosvid.com:	biosvid.asm
	nasm -f bin -o $@ $<
