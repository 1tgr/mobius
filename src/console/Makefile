OBJS=	console.o
TARGET=	$(BIN)/console.exe
LIBS=	$(LIB)/crt0.o $(LIB)/libc.lib $(LIB)/libsys.lib 

include ../make.conf
include ../make.actions

#$(TARGET):	$(OBJS) $(LIBS)
#	$(LD) -o $(TARGET) $(OBJS) $(LIBS)

include $(OBJS:.o=.d)
