include ../make.actions

TARGET=	$(BIN)/tetris.exe
OBJS=	tetris.o ../crt0.o
LIBS=	$(LIB)/libc.lib $(LIB)/kernelu.lib

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	ld --image-base 0x40000000 -o $(TARGET) $(OBJS) $(LIBS)