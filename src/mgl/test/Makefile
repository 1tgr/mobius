include ../../make.actions

TARGET=	$(BIN)/mgltest.exe
OBJS=	mgltest.o ../../wcrt0.o
LIBS=	$(LIB)/mgl.lib $(LIB)/libc.lib $(LIB)/libsys.lib

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	ld -o $(TARGET) $(OBJS) $(LIBS)

include $(OBJS:.o=.d)
