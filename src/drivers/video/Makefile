TARGET=	$(BIN)/video.drv
OBJS=	video.o vga.o vga4.o vga8.o ../tty/map.o vgamodes.o bpp8.o s3.o
BASE=	video
LIBS=	$(LIB)/freetype_s.lib

include ../make.driver

include $(OBJS:.o=.d)
