include ../make.actions

TARGET=	$(BIN)/init.exe
OBJS=	init.o init.res.o
LIBS=	$(LIB)/kernelu.lib $(LIB)/libc.lib

all: $(TARGET)

$(TARGET): $(OBJS)
	ld -o $(TARGET) --subsystem native --image-base 0x40000000 \
		$(OBJS) $(LIBS)