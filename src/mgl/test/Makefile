include ../../make.actions

TARGET=	$(BIN)/mgltest.exe
OBJS=	mgltest.o ../../crt0.o
LIBS=	$(LIB)/mgl.lib $(LIB)/libc.lib $(LIB)/kernelu.lib

all: $(TARGET)

$(TARGET): $(OBJS)
	ld -o $(TARGET) --image-base 0x40000000 \
		$(OBJS) $(LIBS)

include $(OBJS:.o=.d)
