TARGET=	$(BIN)/tetris.exe
OBJS=	tetris.o ../crt0.o
LIBS=	$(LIB)/libc.lib $(LIB)/libsys.lib $(LIB)/mgl.lib

include ../make.actions

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	ld -o $(TARGET) $(OBJS) $(LIBS)