include ../make.actions

TARGET=	$(BIN)/console.exe
OBJS=	console.o
LIBS=	$(LIB)/libc.lib $(LIB)/kernelu.lib

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	ld -o $(TARGET) --subsystem native --image-base 0x40000000 \
		$(OBJS) \
		$(LIBS)