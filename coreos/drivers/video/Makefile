ROOT=	../../..
TARGET_D=	$(BIN)/video.drv
IMP=	$(LIB)/video.lib
DEF=	video.def
EXP=	video.exp

OBJS=	video.o line.o bpp8.o
LIBS=	$(LIB)/kernel.lib
LDFLAGS=--entry _DrvInit
BASE=	video
DEFINES=-DKERNEL

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions

biosvid.h:	biosvid.asm
	nasm -f bin -o biosvid.com biosvid.asm
	./boot2h biosvid.com > biosvid.h
