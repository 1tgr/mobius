TARGET=	$(BIN)/video.drv
OBJS=	video.o vga.o vga4.o vga8.o \
    ../tty/map.o cupertino.o vgamodes.o
BASE=	video

include ../make.driver

include $(OBJS:.o=.d)
