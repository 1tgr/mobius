TARGET=	$(BIN)/video.drv
OBJS=	video.o vga4.o  ../tty/map.o cupertino.o
BASE=	video

include ../make.driver

include $(OBJS:.o=.d)
