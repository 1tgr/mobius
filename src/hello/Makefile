include ../make.actions

TARGET=	$(BIN)/hello.exe
OBJS=	hello.o ../crt0.o
LIBS=	$(LIB)/kernelu.lib $(LIB)/libc.lib

all: $(TARGET)

$(TARGET): $(OBJS)
	ld -o $(TARGET) $(OBJS) $(LIBS)