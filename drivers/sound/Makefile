TARGET=	$(BIN)/sound.drv
OBJS=	sound.o
BASE=	sound
LIBS=	$(LIB)/pci.lib
EXP=	sound.exp
IMP=	$(LIB)/sound.lib

include ../make.driver

include $(OBJS:.o=.d)

subdirs:
	for i in es1371 sb; do make -C $$i; done

all:	subdirs
