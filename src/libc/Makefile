EXP=	libc.exp
DEF=	libc.def
DEFINES=-D__STRICT_ANSI__
BASE=	0x6010000

include ../make.actions

TARGET_D=	$(BIN)/libc.dll
TARGET_S=	$(LIB)/libc_s.lib
IMP=		$(LIB)/libc.lib
LIBS=		$(LIB)/libsys.lib

ASSERT=	
CTYPE=	ctype/ct_flags.o ctype/ct_lower.o ctype/ct_upper.o \
	ctype/isalpha.o ctype/isascii.o ctype/iscntrl.o ctype/isdigit.o \
	ctype/isgraph.o ctype/islower.o ctype/isprint.o ctype/ispunct.o \
	ctype/isspace.o ctype/isupper.o ctype/isxdigit.o ctype/toascii.o \
	ctype/tolower.o ctype/toupper.o
ERRNO=	errno/errno.o
LOCALE=
MALLOC=	malloc-1.18/_emalloc.o malloc-1.18/_malloc.o malloc-1.18/_memalign.o \
	malloc-1.18/_strdup.o malloc-1.18/_strsave.o malloc-1.18/botch.o \
	malloc-1.18/dumpheap.o malloc-1.18/emalloc.o malloc-1.18/getmem.o malloc-1.18/leak.o \
	malloc-1.18/malloc.o malloc-1.18/memalign.o malloc-1.18/setopts.o \
	malloc-1.18/stats.o malloc-1.18/strdup.o malloc-1.18/strsave.o malloc-1.18/verify.o \
	malloc-1.18/splay/sptree.o
MATH=	
STDIO=	stdio/doprintf.o stdio/dowprintf.o stdio/printf.o stdio/sprintf.o \
	stdio/swprintf.o stdio/stdio.o stdio/wprintf.o
STDLIB=	stdlib/abort.o stdlib/qsort.o stdlib/mbtowc.o stdlib/wctomb.o
STRING=	string/mbstowcs.o string/memchr.o string/memcpy.o string/memmove.o \
	string/memset.o string/memcmp.o string/strcat.o string/strchr.o \
	string/strcmp.o string/strcoll.o string/strcpy.o string/strcspn.o \
	string/strerror.o string/stricmp.o string/strlen.o string/strncat.o \
	string/strncmp.o string/strncpy.o string/strpbrk.o string/strrchr.o \
	string/strspn.o string/strstr.o string/strtok.o string/strxfrm.o \
	string/syserr1.o string/syserr2.o string/syserr3.o string/wcscat.o \
	string/wcschr.o string/wcscmp.o string/wcscpy.o string/wcsdup.o \
	string/wcsicmp.o string/wcsmatch.o string/wcsncmp.o string/wcsnicmp.o \
	string/wcsncpy.o string/wcspbrk.o string/wcsrchr.o string/wcslen.o \
	string/wcstombs.o 
TIME=	

RTL=	string/djmd.o string/djmdr.o
SYS=	sys/cputs.o sys/cputws.o sys/exit.o sys/main.o \
	sys/sbrk.o

OBJS=	$(ASSERT) $(CTYPE) $(ERRNO) $(LOCALE) \
	$(MALLOC) $(MATH) $(SETJMP) $(STDARG) \
	$(STDIO) $(STDLIB) $(STRING) $(TIME) \
	$(RTL) $(SYS)

all:	$(TARGET_D) $(IMP) $(TARGET_S)

# Import library for DLL
$(IMP):	$(DEF)
	dlltool -d $< -l $@ -D $(<:.def=.dll)

$(TARGET_S):	$(OBJS)
	ar rcs $(TARGET_S) $(OBJS)

$(TARGET_D):	$(TARGET_S) dllmain.o $(EXP) $(LIBS)
	$(LD) --dll -o $(TARGET_D) --entry _DllMainCRTStartup \
		--image-base $(BASE) \
		$(EXP) dllmain.o \
		$(TARGET_S) $(LIBS)

clean:
	rm $(OBJS:.o=.d)
	rm $(OBJS)
	rm $(EXP)
	rm $(IMP)
	rm $(TARGET_D)
	rm $(TARGET_S)

include $(OBJS:.o=.d)
