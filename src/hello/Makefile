include ../make.actions

TARGET=	$(BIN)/hello.exe
OBJS=	hello.o hello.res.o ../crt0.o
LIBS=	$(LIB)/kernelu.lib $(LIB)/libc.lib

all: $(TARGET)

$(TARGET): $(OBJS)
	ld -o $(TARGET) --image-base 0x40000000 \
		$(OBJS) $(LIBS)