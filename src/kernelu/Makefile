TARGET=	$(BIN)/kernelu.dll
OBJS=	except.o fs.o fullpath.o main.o \
	obj.o proc.o resource.o sys.o \
	thread.o vmm.o device.o debug.o \
	port.o
EXP=	kernelu.exp
IMP=	$(LIB)/kernelu.lib

include ../make.actions

all:	$(TARGET) $(IMP)

$(TARGET):	$(OBJS) $(EXP) kernelu.res.o Makefile
	ld -o $(TARGET) \
		$(OBJS) $(EXP) ../crt0dll.o kernelu.res.o \
		$(LIB)/libcs.lib \
		--image-base 0x10030000 --entry _DllMainCRTStartup@12

%.exp: %.def
	dlltool -e $@ -m i386 --input-def $< -D $(EXP:.exp=.dll)

$(LIB)/%.lib: %.def Makefile
	dlltool -l $@ -m i386 --input-def $< -D $(EXP:.exp=.dll)

include $(OBJS:.o=.d)
