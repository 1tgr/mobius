TARGET=	$(BIN)/keyboard.drv
OBJS=	keyboard.o
BASE=	keyboard
LIBS=	$(LIB)/tty.lib

MAPS=	$(BIN)/british.kbd $(BIN)/russian.kbd

include ../make.driver

$(BIN)/%.kbd: %.o
	objcopy -O binary -j .data $< $@

include $(OBJS:.o=.d)

all: $(MAPS)
