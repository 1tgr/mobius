TARGET=	$(BIN)/shell.exe
OBJS=	shell.o shell.res.o ../crt0.o
LIBS=	$(LIB)/librtl_s.lib $(LIB)/libc_s.lib $(LIB)/libsys.lib

include ../make.actions

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	ld -o $(TARGET) $(OBJS) $(LIBS)