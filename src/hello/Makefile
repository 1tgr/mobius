TARGET=	$(BIN)/hello.exe
OBJS=	hello.o ../crt0.o
LIBS=	$(LIB)/librtl_s.lib $(LIB)/libc_s.lib $(LIB)/libsys.lib

include ../make.actions

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	ld -o $(TARGET) $(OBJS) $(LIBS)