ROOT=	../../..
TARGET=	$(BIN)/keyboard.drv
OBJS=	keyboard.o
BASE=	keyboard

MAPS=	$(BIN)/british.kbd $(BIN)/russian.kbd $(BIN)/us.kbd

include $(ROOT)/coreos/drivers/make.driver

$(BIN)/%.kbd: $(OBJDIR)/%.o
	objcopy -O binary -j .data $< $@

all: $(MAPS)
