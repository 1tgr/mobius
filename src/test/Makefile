OBJS=	test.o ../crt0.o
TARGET=	$(BIN)/test.exe
LIBS=	$(LIB)/libc_s.lib $(LIB)/libsys.lib 

include ../make.actions

$(TARGET):	$(OBJS) $(LIBS)
	$(LD) -o $(TARGET) $(OBJS) $(LIBS)

clean:
	rm $(OBJS:.o=.d)
	rm $(OBJS)
	rm $(TARGET)

include $(OBJS:.o=.d)
