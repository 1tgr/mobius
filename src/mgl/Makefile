TARGET_D=	$(BIN)/mgl.dll
TARGET_S=	$(LIB)/mgl_s.lib
OBJS=	rc.o render.o rect.o wmf.o mgl.res.o dllmain.o
DEF=	mgl.def
IMP=	$(LIB)/mgl.lib
CFLAGS=	-g
LIBS=	$(LIB)/libc.lib $(LIB)/libsys.lib
BASE=	mgl

include ../make.conf
include ../make.actions

all:
	make -C test
