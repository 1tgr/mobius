EXP=	libc.exp
DEF=	libc.def
#DEFINES=-D__STRICT_ANSI__
DEFINES=-D__LIBC_STATIC
BASE=	libc

include ../make.actions

TARGET_D=	$(BIN)/libc.dll
TARGET_S=	$(LIB)/libc_s.lib
IMP=		$(LIB)/libc.lib
LIBS=		$(LIB)/libsys.lib

ASSERT=	
CTYPE=	ctype/ct_flags.o ctype/ct_lower.o ctype/ct_upper.o \
	ctype/isalpha.o ctype/isalnum.o ctype/isascii.o ctype/iscntrl.o \
	ctype/isdigit.o ctype/isgraph.o ctype/islower.o ctype/isprint.o \
	ctype/ispunct.o ctype/isspace.o ctype/isupper.o ctype/isxdigit.o \
	ctype/toascii.o ctype/tolower.o ctype/toupper.o
ERRNO=	errno/errno.o
LOCALE=
MALLOC=	malloc-1.18/_emalloc.o malloc-1.18/_malloc.o malloc-1.18/_memalign.o \
	malloc-1.18/_strdup.o malloc-1.18/_strsave.o malloc-1.18/botch.o \
	malloc-1.18/dumpheap.o malloc-1.18/emalloc.o malloc-1.18/getmem.o malloc-1.18/leak.o \
	malloc-1.18/malloc.o malloc-1.18/memalign.o malloc-1.18/setopts.o \
	malloc-1.18/stats.o malloc-1.18/strdup.o malloc-1.18/strsave.o malloc-1.18/verify.o \
	malloc-1.18/sptree.o
MATH=	
STDIO=	stdio/allocfil.o stdio/clearerr.o stdio/doprintf.o stdio/dowprintf.o \
	stdio/doscan.o \
	stdio/fclose.o stdio/feof.o stdio/ferror.o stdio/fflush.o \
	stdio/fgetc.o stdio/fgetpos.o stdio/fgets.o stdio/filbuf.o \
	stdio/flsbuf.o stdio/fopen.o stdio/fprintf.o stdio/fputc.o \
	stdio/fputs.o stdio/fread.o stdio/freopen.o stdio/frlist.o \
	stdio/fscanf.o stdio/fseek.o stdio/fsetpos.o stdio/ftell.o \
	stdio/fwalk.o stdio/fwrite.o stdio/getc.o stdio/getchar.o \
	stdio/gets.o stdio/getw.o stdio/perror.o stdio/printf.o \
	stdio/putc.o stdio/putchar.o stdio/puts.o stdio/putw.o \
	stdio/remove.o stdio/rename.o stdio/rewind.o stdio/scanf.o \
	stdio/setbuf.o stdio/setbuffe.o stdio/setlineb.o stdio/setvbuf.o \
	stdio/sprintf.o stdio/sscanf.o stdio/stdaux.o stdio/stderr.o \
	stdio/stdin.o stdio/stdiohk.o stdio/stdio.o stdio/stdout.o \
	stdio/stdprn.o stdio/swprintf.o stdio/tmpfile.o stdio/tmpnam.o \
	stdio/ungetc.o stdio/vfprintf.o stdio/vprintf.o stdio/vsprintf.o \
	stdio/wprintf.o
STDLIB=	stdlib/abort.o stdlib/abs.o stdlib/atexit.o stdlib/atof.o \
	stdlib/atoi.o stdlib/atol.o stdlib/atold.o stdlib/bsdqsort.o \
	stdlib/bsearch.o stdlib/div.o stdlib/exit.o \
	stdlib/labs.o stdlib/ldiv.o stdlib/llabs.o stdlib/lldiv.o \
	stdlib/qsort.o stdlib/rand.o stdlib/strtod.o stdlib/strtol.o \
	stdlib/strtold.o stdlib/strtoll.o stdlib/strtoul.o stdlib/strtoull.o
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
TIME=	time/localtime.o time/time.o

RTL=	string/djmd.o string/djmdr.o
SYS=	sys/access.o sys/close.o sys/cputs.o sys/cputws.o \
	sys/creatnew.o sys/float_dx.o sys/fmode.o sys/getenv.o \
	sys/huge_val.o sys/isatty.o \
	sys/lseek.o sys/main.o sys/open.o sys/read.o \
	sys/sbrk.o sys/towc.o sys/write.o

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

$(TARGET_D):	$(TARGET_S) dllmain.o $(EXP) $(LIBS) .base
	$(LD) -g --dll -o $(TARGET_D) --entry _DllMainCRTStartup \
		--image-base `cat .base` \
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
