EXP=	libc.exp
DEF=	libc.def
#DEFINES=-D__STRICT_ANSI__
DEFINES=-D__LIBC_STATIC -DSTDHEADERS
BASE=	libc

include ../make.conf

TARGET_D=	$(BIN)/libc.dll
TARGET_S=	$(LIB)/libc_s.lib
IMP=		$(LIB)/libc.lib
LIBS=		$(LIB)/libsys.lib

ASSERT=	assert/assert.o
CTYPE=	ctype/ct_flags.o ctype/ct_lower.o ctype/ct_upper.o \
	ctype/isalpha.o ctype/isalnum.o ctype/isascii.o ctype/iscntrl.o \
	ctype/isdigit.o ctype/isgraph.o ctype/islower.o ctype/isprint.o \
	ctype/ispunct.o ctype/isspace.o ctype/isupper.o ctype/isxdigit.o \
	ctype/toascii.o ctype/tolower.o ctype/toupper.o \
	ctype/towupper.o ctype/towlower.o ctype/iswupper.o ctype/iswlower.o
ERRNO=	errno/errno.o
LOCALE=
#MALLOC=	malloc-1.18/_emalloc.o malloc-1.18/_malloc.o malloc-1.18/_memalign.o \
	malloc-1.18/_strdup.o malloc-1.18/_strsave.o malloc-1.18/botch.o \
	malloc-1.18/dumpheap.o malloc-1.18/emalloc.o malloc-1.18/getmem.o malloc-1.18/leak.o \
	malloc-1.18/malloc.o malloc-1.18/memalign.o malloc-1.18/setopts.o \
	malloc-1.18/stats.o malloc-1.18/strdup.o malloc-1.18/strsave.o malloc-1.18/verify.o \
	malloc-1.18/sptree.o
MALLOC=	stdlib/malloc.o stdlib/maldebug.o stdlib/malwrap.o
MATH=	math/acos.o math/acosh.o math/asin.o math/asinh.o \
	math/atan.o math/atan2.o math/atanh.o \
	math/ceil.o math/cos.o math/cosh.o math/exp.o \
	math/fabs.o math/floor.o math/fmod.o math/frexp.o \
	math/ftol.o math/hypot.o math/j0_y0.o \
	math/j1_y1.o math/jn_yn.o math/ldexp.o math/log.o math/log10.o \
	math/modf.o math/pow.o math/sin.o math/sinh.o \
	math/sqrt.o math/tan.o math/tanh.o \
	math/isnan.o
STDIO=	stdio/allocfil.o stdio/clearerr.o stdio/doprintf.o stdio/dowprintf.o \
	stdio/doscan.o \
	stdio/fclose.o stdio/fdopen.o stdio/feof.o stdio/ferror.o stdio/fflush.o \
	stdio/fgetc.o stdio/fgetpos.o stdio/fgets.o stdio/filbuf.o \
	stdio/flsbuf.o stdio/fopen.o stdio/fprintf.o stdio/fputc.o \
	stdio/fputs.o stdio/fread.o stdio/freopen.o stdio/frlist.o \
	stdio/fscanf.o stdio/fseek.o stdio/fsetpos.o stdio/ftell.o \
	stdio/fwalk.o stdio/fwprintf.o stdio/fwrite.o stdio/getc.o \
	stdio/getchar.o stdio/gets.o stdio/getw.o stdio/perror.o \
	stdio/pwerror.o stdio/printf.o \
	stdio/putc.o stdio/putchar.o stdio/puts.o stdio/putw.o \
	stdio/remove.o stdio/rename.o stdio/rewind.o stdio/scanf.o \
	stdio/setbuf.o stdio/setbuffe.o stdio/setlineb.o stdio/setvbuf.o \
	stdio/sprintf.o stdio/sscanf.o stdio/stdaux.o stdio/stderr.o \
	stdio/stdin.o stdio/stdiohk.o stdio/stdio.o stdio/stdout.o \
	stdio/stdprn.o stdio/swprintf.o stdio/tmpfile.o stdio/tmpnam.o \
	stdio/ungetc.o \
	stdio/wprintf.o stdio/wfopen.o stdio/fputwc.o stdio/fputws.o \
	stdio/getwc.o
STDLIB=	stdlib/abort.o stdlib/abs.o stdlib/atexit.o stdlib/atof.o \
	stdlib/atoi.o stdlib/atol.o stdlib/atold.o \
	stdlib/bsearch.o stdlib/cppsup.o stdlib/div.o stdlib/exit.o \
	stdlib/labs.o stdlib/ldiv.o stdlib/llabs.o \
	stdlib/qsort.o stdlib/rand.o stdlib/strtod.o stdlib/strtol.o \
	stdlib/strtold.o stdlib/strtoll.o stdlib/strtoul.o stdlib/strtoull.o \
	stdlib/wcstol.o stdlib/mbtowc.o stdlib/wctomb.o stdlib/wcstoul.o 
STRING=	string/mbstowcs.o string/memchr.o string/memcpy.o string/memmove.o \
	string/memset.o string/memcmp.o string/strcat.o string/strchr.o \
	string/strcmp.o string/strcoll.o string/strcpy.o string/strcspn.o \
	string/strdup.o string/strerror.o string/stricmp.o string/strlen.o string/strncat.o \
	string/strncmp.o string/strncpy.o string/strpbrk.o string/strrchr.o \
	string/strspn.o string/strstr.o string/strtok.o string/strxfrm.o \
	string/syserr1.o string/syserr2.o string/syserr3.o  string/syswerr1.o \
	string/syswerr2.o string/syswerr3.o string/wcscat.o \
	string/wcschr.o string/wcscmp.o string/wcscpy.o string/wcsdup.o \
	string/wcserror.o string/wcsicmp.o string/wcsmatch.o string/wcsncmp.o \
	string/wcsnicmp.o string/wcsncpy.o string/wcspbrk.o string/wcsrchr.o \
	string/wcslen.o string/wcstombs.o string/wcwidth.o
TIME=	time/localtime.o time/time.o

RTL=	string/djmd.o string/djmdr.o dllmain.o
SYS=	sys/cputs.o sys/cputws.o sys/float_dx.o sys/fmode.o \
	sys/getenv.o sys/huge_val.o sys/int64.o sys/main.o \
	sys/morecore.o sys/towc.o sys/wdprintf.o sys/unicode.o \
	sys/ucfile.o \
	crt0/args.o crt0/wargs.o

CRT0=	$(LIB)/crt0.o $(LIB)/wcrt0.o

OBJS=	$(ASSERT) $(CTYPE) $(ERRNO) $(LOCALE) \
	$(MALLOC) $(MATH) $(SETJMP) $(STDARG) \
	$(STDIO) $(STDLIB) $(STRING) $(TIME) \
	$(RTL) $(SYS)

include ../make.actions

all:	$(BIN)/unicode.dat $(CRT0)

$(LIB)/crt0.o:	crt0/crt0.c
	$(CC) -c $< -o $@ -I$(INCLUDE) -I./include $(CFLAGS)

$(LIB)/wcrt0.o:	crt0/crt0.c
	$(CC) -c $< -o $@ -I$(INCLUDE) -I./include -DWIDE $(CFLAGS)

crt0/args.o:	crt0/args.c
	$(CC) -c $< -o $@ -I$(INCLUDE) -I./include $(CFLAGS)

crt0/wargs.o:	crt0/args.c
	$(CC) -c $< -o $@ -I$(INCLUDE) -I./include -DWIDE $(CFLAGS)

crt0/wargs.d:	crt0/args.c
	 echo "$(dir $<)`$(CC) -c -M $< -I$(INCLUDE) -I./include -DWIDE $(CFLAGS)`" > $@

$(BIN)/unicode.dat:	ctype/unicode.o
	objcopy -O binary -j .data $< $@
