TARGET_D=	$(BIN)/sb.kll
OBJS=	sb.o
BASE=	sb
LIBS=	$(LIB)/pci.lib $(LIB)/kernel.lib $(LIB)/sound.lib
DEFINES=-DKERNEL

include ../../../make.conf
include ../../../make.actions

include $(OBJS:.o=.d)
