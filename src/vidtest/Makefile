TARGET=	$(BIN)/vidtest.exe
OBJS=	vidtest.o
LIBS=	$(LIB)/crt0.o $(LIB)/libc_s.lib $(LIB)/libsys.lib

include ../make.conf
include ../make.actions

#all: $(TARGET)

#$(TARGET): $(OBJS) $(LIBS)
#	ld -o $(TARGET) $(OBJS) $(LIBS)

include $(OBJS:.o=.d)
