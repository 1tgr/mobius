STATIC=	$(LIB)/libc_s.lib
DLL=	$(BIN)/libc.dll
IMP=	$(LIB)/libc.lib
EXP=	libc.exp
CFLAGS=	-g

STARTUP=init.o
CONIO=	conio/kbhit.o conio/wgetch.o conio/wgetche.o
#MALLOC=	malloc/free.o malloc/malloc.o malloc/msize.o malloc/realloc.o

MALLOC=	malloc-1.18/_emalloc.o malloc-1.18/_malloc.o malloc-1.18/_memalign.o \
	malloc-1.18/_strdup.o malloc-1.18/_strsave.o malloc-1.18/botch.o \
	malloc-1.18/dumpheap.o malloc-1.18/emalloc.o malloc-1.18/getmem.o malloc-1.18/leak.o \
	malloc-1.18/malloc.o malloc-1.18/memalign.o malloc-1.18/setopts.o \
	malloc-1.18/stats.o malloc-1.18/strdup.o malloc-1.18/strsave.o malloc-1.18/verify.o \
	malloc-1.18/splay/sptree.o

STDIO=	stdio/doprintf.o stdio/dowprintf.o stdio/fclose.o stdio/fopen.o \
	stdio/fread.o stdio/fseek.o stdio/ftell.o stdio/pwerror.o \
	stdio/sprintf.o stdio/swprintf.o stdio/wprintf.o
STDLIB=	stdlib/errno.o stdlib/qsort.o stdlib/rand.o stdlib/srand.o stdlib/strtol.o
STRING=	string/iswlower.o string/iswupper.o string/match.o string/mbstowcs.o \
	string/memcpy.o string/memmove.o string/memset.o string/strcmp.o \
	string/strcpy.o string/stricmp.o string/strlen.o string/strncmp.o \
	string/strncpy.o string/strpbrk.o string/towlower.o string/towupper.o \
	string/unicode.o string/wcscat.o string/wcschr.o string/wcscmp.o \
	string/wcscpy.o string/wcsdup.o string/wcserror.o string/wcsicmp.o \
	string/wcslen.o string/wcsncpy.o  string/wcspbrk.o string/wcsrchr.o \
	string/wcstombs.o
SYS=	sys/atexit.o sys/core.o sys/cputws.o

OBJS=	$(STARTUP) $(CONIO) $(STDIO) $(STDLIB) $(STRING) $(SYS)
DLLOBJS=dllmain.o ../crt0dll.o

LIBS=	$(MALLOC)
DLLLIBS=	$(LIB)/kernelu.lib $(LIBS)

include ../make.actions

all:	$(STATIC) $(DLL) $(IMP)

malloc-1.18/libmalloc_d.a:
	make -C malloc-1.18 --unix

$(STATIC): $(OBJS)
	ar -rcs $(STATIC) $(OBJS) $(LIBS)

$(DLL):	$(DLLOBJS) $(OBJS) $(EXP) $(LIBS) Makefile
	ld -g -o $(DLL) --entry _DllMain@12 --image-base 0x10000000 \
		$(DLLOBJS) \
		$(OBJS) \
		$(EXP) $(DLLLIBS)

%.exp: %.def
	dlltool -e $@ -m i386 --input-def $< -D $(EXP:.exp=.dll)

$(LIB)/%.lib: %.def Makefile
	dlltool -l $@ -m i386 --input-def $< -D $(EXP:.exp=.dll)

include $(OBJS:.o=.d)
