OBJS=	console.o ../crt0.o
TARGET=	$(BIN)/console.exe
LIBS=	$(LIB)/libc_s.lib $(LIB)/libsys_s.lib 

include ../make.conf
include ../make.actions

#$(TARGET):	$(OBJS) $(LIBS)
#	$(LD) -o $(TARGET) $(OBJS) $(LIBS)

include $(OBJS:.o=.d)
