TARGET_D=	$(BIN)/es1371.kll
OBJS=	es1371.o
BASE=	es1371
LIBS=	$(LIB)/pci.lib $(LIB)/kernel.lib $(LIB)/sound.lib
DEFINES=-DKERNEL

include ../../../make.conf
include ../../../make.actions

include $(OBJS:.o=.d)
