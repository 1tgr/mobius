#
# Floppy Controller code makefile
#

TARGET=	$(BIN)/fdc.drv
OBJS=	fdc.o util.o
BASE=	@$(BIN)/coffbase.txt,fdc

include ../make.driver

include $(OBJS:.o=.d)
