TARGET=	$(BIN)/mgl.dll
OBJS=	init.o rc.o render.o
EXP=	mgl.exp
IMP=	$(LIB)/mgl.lib
CFLAGS=	-g
LIBS=	$(LIB)/freetype.lib $(LIB)/libc_s.lib $(LIB)/kernelu.lib

include ../make.actions

all:	$(TARGET) $(IMP)
	make -C test

$(TARGET):	$(OBJS) $(EXP) $(LIBS) mgl.res.o Makefile
	ld -o $(TARGET) -g \
		$(OBJS) $(EXP) ../crt0dll.o mgl.res.o \
		$(LIBS) \
		--image-base 0x10050000 --entry _DllMainCRTStartup@12

include $(OBJS:.o=.d)
