TARGET=	$(BIN)/video.drv
OBJS=	video.o bpp8.o vga.o vga4.o vga8.o \
    vgamodes.o s3.o
# polygon.o line.o text.o
BASE=	video
#LIBS=	freetype_s.lib
#CFLAGS=	-Ifreetype2/include

#all:
#	make -C freetype2

include ../make.driver

include $(OBJS:.o=.d)
