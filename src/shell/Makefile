TARGET=	$(BIN)/shell.exe
OBJS=	shell.o line.o params.o v86.o \
    shell.res.o ../crt0.o
LIBS=	$(LIB)/librtl_s.lib $(LIB)/libc.lib $(LIB)/libsys.lib $(LIB)/libiconv.lib

include ../make.actions

all: $(BIN)/v86.com $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	ld -o $(TARGET) $(OBJS) $(LIBS)

include $(OBJS:.o=.d)

$(BIN)/v86.com: v86.asm
	nasm -f bin $< -o `cygpath -w $@`
