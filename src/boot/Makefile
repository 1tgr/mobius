BIN=	..\..\bin
TARGET=	$(BIN)\mobel_pe.com
BOOTSECT=	$(BIN)\bootsect.com
OBJS=	startup.obj main.obj io.obj fat.obj loaderb.obj
LIBPATH=f:\tc\lib
CFLAGS=	-w -mt -O2 -d -Z -1 -vi
LFLAGS	=/x /c

all:	$(TARGET) $(BOOTSECT)

$(TARGET):	$(OBJS) Makefile
	tlink $(LFLAGS) /t $(OBJS),$(TARGET),,$(LIBPATH)\cs.lib
	ndisasmw $(TARGET) > listing.txt

%.obj:	%.c Makefile
	tcc -c $(CFLAGS) -o$@ $<

%.obj:	%.asm
	nasm -DNODOS -f obj -o$@ $<

$(BOOTSECT):	bootsect.asm
	nasm -f bin -o$@ $<

floppy:	$(TARGET)
	copy $(TARGET) a:

bochs:	$(TARGET)
	make -C ../../bin