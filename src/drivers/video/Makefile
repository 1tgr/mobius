TARGET=	$(BIN)/video.drv
OBJS=	video.o vga.o vga4.o vga8.o \
    ../tty/map.o vgamodes.o cupertino.o font8x8.o font1.o lucon.o
BASE=	video

include ../make.driver

include $(OBJS:.o=.d)
