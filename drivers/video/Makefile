TARGET=	$(BIN)/video.drv
OBJS=	video.o bpp8.o vga.o vga8.o \
	vgamodes.o s3.o
BASE=	video

include ../make.driver
include $(OBJS:.o=.d)
