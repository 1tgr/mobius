TARGET=	$(BIN)/devtest.exe
OBJS=	devtest.o system.o sys.o
LIBS=	$(LIB)/libcs.lib $(LIB)/kernelu.lib

include ../make.actions

all:	$(TARGET)

%.o:	%.c
	cl /nologo /Zl $< /Fo$@

%.s1:	%.pas
	ppc386 $< -g -s -a -TGO32V1 -Anasmobj

system.o:	system.pas
	ppc386 system.pas -g -s -a -Us -TGO32V1 -Anasmobj
	$(NASM) -f win32 system.s1 -o system.o

%.o:	%.s1
	$(NASM) -f win32 $< -o $@

$(TARGET): $(OBJS) Makefile
#	link /nologo /libpath:$(LIB) /base:0x40000000 \
#		/out:$(TARGET) \
#		/subsystem:native /entry:main /nodefaultlib /map:devtest.map \
#		$(OBJS) \
#		kernelu.lib libc.lib
	ld -o $(TARGET) --image-base 0x40000000 --subsystem native --entry _mainCRTStartup \
		../crt0.o $(OBJS) \
		$(LIBS)

clean:
	rm $(TARGET)
	rm $(OBJS)
	rm $(OBJS:.o=.s1)
