TARGET=	$(BIN)/video.drv
OBJS=	video.o polygon.o line.o text.o \
    bpp8.o vga.o vga4.o vga8.o \
    vgamodes.o s3.o
BASE=	video
LIBS=	$(LIB)/freetype_s.lib

include ../make.driver

include $(OBJS:.o=.d)
