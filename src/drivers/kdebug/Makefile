TARGET=	$(BIN)/kdebug.dll
OBJS=	kdebug.o disasm.o insnsd.o sync.o
EXP=	kdebug.exp
IMP=	$(LIB)/kdebug.lib
BASE=	@$(BIN)/coffbase.txt,kdebug

include ../make.driver

%.exp: %.def
	dlltool -e $@ -m i386 --input-def $< -D $(@:.exp=.dll)

$(LIB)/%.lib: %.def
	lib /def:$< /out:$@ /nologo /machine:ix86

include $(OBJS:.o=.d)