TARGET=	$(BIN)/short.exe
OBJS=	short.o 
LIBS=	$(LIB)/libcs.lib $(LIB)/kernelu.lib

include ..\make.actions

all:	$(TARGET)

$(TARGET):	$(OBJS) ../crt0.o short.res.o $(LIBS)
#	link /out:$(TARGET) $(OBJS) /subsystem:console /nodefaultlib \
#		../crt0.o $(LIB)/libc.lib $(LIB)/kernelu.lib
	ld -o $(TARGET) \
		$(OBJS) ../crt0.o short.res.o \
		$(LIBS) \
		--subsystem console --image-base 0x40000000

include $(OBJS:.o=.d)