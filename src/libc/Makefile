STATIC=	$(LIB)/libcs.lib
DLL=	$(BIN)/libc.dll
IMP=	$(LIB)/libc.lib
EXP=	libc.exp

STARTUP=init.o
CONIO=	conio/kbhit.o conio/wgetch.o conio/wgetche.o
MALLOC=	malloc/free.o malloc/malloc.o malloc/msize.o malloc/realloc.o
STDIO=	stdio/dowprintf.o stdio/pwerror.o stdio/swprintf.o stdio/wprintf.o
STDLIB=	stdlib/errno.o stdlib/rand.o stdlib/srand.o stdlib/strtol.o
STRING=	string/iswlower.o string/iswupper.o string/match.o string/mbstowcs.o \
	string/memcpy.o string/memmove.o string/memset.o string/strcmp.o \
	string/stricmp.o string/strlen.o string/strncmp.o string/strpbrk.o \
	string/towlower.o string/towupper.o string/unicode.o string/wcscat.o \
	string/wcschr.o string/wcscmp.o string/wcscpy.o string/wcsdup.o \
	string/wcserror.o string/wcsicmp.o string/wcslen.o string/wcsncpy.o \
	string/wcspbrk.o string/wcsrchr.o
SYS=	sys/atexit.o sys/core.o sys/cputws.o

OBJS=	$(STARTUP) $(CONIO) $(MALLOC) $(STDIO) $(STDLIB) $(STRING) $(SYS)
DLLOBJS=dllmain.o ../crt0dll.o

LIBS=	$(LIB)/kernelu.lib

include ../make.actions

all:	$(STATIC) $(DLL) $(IMP)

$(STATIC): $(OBJS)
	ar -rcs $(STATIC) $(OBJS)

$(DLL):	$(DLLOBJS) $(OBJS) $(EXP) $(LIBS) Makefile
	ld -o $(DLL) --entry _DllMain@12 --image-base 0x10000000 \
	$(DLLOBJS) \
	$(OBJS) \
	$(EXP) $(LIBS)

%.exp: %.def
	dlltool -e $@ -m i386 --input-def $< -D $(EXP:.exp=.dll)

$(LIB)/%.lib: %.def Makefile
	dlltool -l $@ -m i386 --input-def $< -D $(EXP:.exp=.dll)

include $(OBJS:.o=.d)
