TARGET=	$(BIN)/devtest.exe
OBJS=	devtest.o ../crt0.o
LIBS=	$(LIB)/libc.lib $(LIB)/kernelu.lib

include ../make.actions

all:	$(TARGET)

$(TARGET):	$(OBJS) $(LIBS) Makefile
#	link /nologo /libpath:$(LIB) /base:0x40000000 /debug /debugtype:coff \
#		/out:$(TARGET) \
#		/subsystem:native /entry:NtProcessStartup /nodefaultlib \
#		$(OBJS) $(LIBS)
	ld -g -L $(LIB) --image-base 0x40000000 --subsystem native \
	  --entry _mainCRTStartup -o $(TARGET) \
	  $(OBJS) $(LIBS)

clean:
	rm $(TARGET)
	rm $(OBJS)

include $(OBJS:.o=.d)