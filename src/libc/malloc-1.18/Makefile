# This Makefile is set up to make a debugging malloc called libmalloc_d.a
# and a non-debugging fast malloc called libmalloc.a.
# Also generates testmalloc, simumalloc, teststomp and maltrace (together
# with _d versions)
#
# 'make veryclean' cleans out lib*.a as well.
#
# 'make .lint' runs lint.
#
# 'make install' puts libmalloc.a and libmalloc_d.a in $LIBDIR and
# puts malloc.h in $INCDIR.
#

include ../../make.actions

LIBDIR=$(HOME)/lib/$(ARCH)
INCDIR=$(HOME)/include

# neutralize SystemV genius
SHELL=/bin/sh

# DEBUGDEFS are set for libmalloc_d.a. Say 'make libmalloc' for nondebug
# version. (DEBUGDEFS=$(FASTDEFS))
#
DEBUGDEFS=-DDEBUG -DTRACE -DPROFILESIZES

# FASTDEFS are used when the 'libmalloc' target is made.
#
# -DTRACE and -DPROFILESIZES shouldn't introduce very much overhead since the
# former is turned off (one 'if'), and the latter is one 'if' + increment.
# So you can keep them even in the fast production version. 
# You may want to define -DSVID_MALLOC_0 if you want malloc(0) to
# return NULL, per the SVID.  Otherwise, I return a block that
# has zero size so it can then be passed to realloc().
#
FASTDEFS=#-DSVID_MALLOC_0 -DTRACE -DPROFILESIZES


#  NORMALDEFS are used for both debugging and non-debugging versions
# -DSHORTNAMES makes sure all internal global symbols are unique within 6
# characters. Avoid defining unless your linker is braindead.
# -DUSESTDIO is to make this use fputs() instead of write() for trace
# and debugging output. write() is preferable since it is unbuffered,
# and does not call malloc() or suchlike. Avoid defining if possible.
# -DSTDHEADERS if you have ANSI standard header files (stdlib.h, string.h)
# This can be defined on Solaris2.1, Irix3.3.x, BSD3.3,
# 386BSD, BSD386 and other sufficiently Posix systems.
# -DHAVE_MMAP can be defined for SunOS4.x and other systems
# that have a general purpose mmap call that allows memory-mapped files.
#
NORMALDEFS=-DSTDHEADERS #-DHAVE_MMAP # -DSHORTNAMES -DUSESTDIO

# CC = gcc -ansi -Wall -O # -pedantic # add -pedantic if you fixed your includes.
# SGI needs cc -xansi -D__STDC__ on Irix4.0.5.

CDEBUGFLAGS=-g
EXTRAINCLUDES=-I$(HOME)/include
LINT=lint
LN = ln -s
OLDCC = cc
OLDCFLAGS = -O
AR = ar
ARFLAGS = ruv
RANLIB = ranlib

LDFLAGS=#-Bstatic

# only developers should have to change stuff below this line

EXT=_d
LIBMALLOC=libmalloc$(EXT).a
#PROGS=testmalloc$(EXT) simumalloc$(EXT) teststomp$(EXT) maltrace$(EXT)
PROGS=

DEFINES= $(NORMALDEFS) $(DEBUGDEFS)

SPLAYOBJ = splay/sptree.o
SPLAYSRC = splay/sptree.c
SPLAYHDR = splay/sptree.h

SRCS =  _emalloc.c _malloc.c _memalign.c \
	_strdup.c _strsave.c botch.c \
	dumpheap.c emalloc.c getmem.c leak.c \
	malloc.c memalign.c setopts.c \
	stats.c strdup.c strsave.c verify.c

OBJS =  _emalloc.o _malloc.o _memalign.o \
	_strdup.o _strsave.o botch.o \
	dumpheap.o emalloc.o getmem.o leak.o \
	malloc.o memalign.o setopts.o \
	stats.o strdup.o strsave.o verify.o

# HDRS, DOCS, TESTS and EXTRAS are used when making distributions.
# so please keep them uptodate.
# bundle is smart enough not to include object files, RCS, executables,
# etc, and does subdirectories right, but there's often other files
# in the development directory...

# globals.c, version.c are included in malloc.c.
HDRS = align.h assert.h defs.h externs.h globals.c globals.h globrename.h \
	malloc.h trace.h version.c

DOCS = README NOTE TODO CHANGES malloc.doc Makefile

TESTS = testmalloc.c test.out teststomp.c tests regress \
	simumalloc.c

EXTRAS = splay

INCLUDES=-I./splay $(EXTRAINCLUDES)

CFLAGS += $(CDEBUGFLAGS) $(INCLUDES) $(DEFINES)

all: pass clean libmalloc

pass: $(LIBMALLOC) $(PROGS) out$(EXT)

libmalloc:
	$(MAKE) -f Makefile $(MFLAGS) CC="$(CC)" DEBUGDEFS="$(FASTDEFS)" \
		EXT="" CDEBUGFLAGS="$(CDEBUGFLAGS)" \
		NORMALDEFS="$(NORMALDEFS)" pass

testmalloc$(EXT): testmalloc.c .lib$(EXT)
	$(CC) $(CFLAGS) -o $@ testmalloc.c $(LIBMALLOC) ${LDFLAGS}

teststomp$(EXT): teststomp.c .lib$(EXT)
	$(CC) $(CFLAGS) -o $@ teststomp.c $(LIBMALLOC) ${LDFLAGS}

simumalloc$(EXT): simumalloc.c .lib$(EXT)
	$(CC) $(CFLAGS) -DMYMALLOC -o $@ simumalloc.c $(LIBMALLOC) ${LDFLAGS}

maltrace$(EXT): maltrace.c .lib$(EXT)
	$(CC) $(CFLAGS) -DMYMALLOC -o $@ maltrace.c $(LIBMALLOC) ${LDFLAGS}

$(LIBMALLOC): $(OBJS) $(SPLAYOBJ)
	rm -f $(LIBMALLOC)
	$(AR) $(ARFLAGS) $(LIBMALLOC) $(OBJS) $(SPLAYOBJ)
	-$(RANLIB) $(LIBMALLOC)
	touch .lib$(EXT)

$(SPLAYOBJ): .foo
	cd splay; $(MAKE) $(MFLAGS) DEFINES="$(DEFINES)" \
		LIBMALLOC=../$(LIBMALLOC) CC="$(CC)"

out$(EXT): $(PROGS)
	rm -f out$(EXT)
	EXT=$(EXT); export EXT; ./regress > out$(EXT) 2>&1
	@echo "Ok to get two core dumps if this is teststomp_d"
	-(./teststomp$(EXT) 5; ./teststomp$(EXT) 8) >>out$(EXT) 2>&1

one: onefile.o

onefile.c: $(SRCS) $(SPLAYSRC)
	rm -f onefile.c
	cat $(SRCS) $(SPLAYSRC) | sed '/RCSID/d' > onefile.c
	
.lint: $(SRCS)
	($(LINT) $(LINTFLAGS) $(DEFINES) $(INCLUDES) $(SRCS); \
		$(LINT) $(LINTFLAGS) $(DEFINES) $(INCLUDES) -I.. $(SPLAYSRC)) \
		2>&1 | egrep -v 'pointer alignment|illegal pointer comb'
	touch .lint

.foo:

cleanprogs:
	-rm -f $(PROGS) $(LIBMALLOC) out$(EXT) trace$(EXT) .lib$(EXT)

clean:
	-rm -f *.o \#* *~ core a.out gmon.out mon.out onefile.c *.sL prof.out
	cd splay; $(MAKE) clean

veryclean: clean cleanprogs
	make EXT= cleanprogs
	-rm -f make.log make.out o core simumalloc_*

install:
	install -c -m 644 libmalloc.a $(LIBDIR)
	-$(RANLIB) $(LIBDIR)/libmalloc.a
	install -c -m 644 libmalloc_d.a $(LIBDIR)
	-$(RANLIB) $(LIBDIR)/libmalloc_d.a
	install -c -m 644 malloc.h $(INCDIR)
	
.id: $(SRCS)
	mkid $(SRCS) $(SPLAYSRC) $(HDRS) $(SPLAYHDR)
	touch .id

dist:
	@rm -f Makefile.bak
	@mv Makefile Makefile.bak;\
	sed '/^# DO NOT PUT ANYTHING/,$$d' Makefile.bak > Makefile; \
	(bundle -v $(DOCS) $(SRCS) $(HDRS) $(TESTS) $(EXTRAS)); \
	mv Makefile.bak Makefile

files:
	find * -type f -print | \
		egrep -v '(,v|\.o|core|make.log|simumalloc|testmalloc|teststomp)$$' | \
		egrep -v '(libmalloc.*\.a|res\..*)$$' > FILES

.dep: $(SRCS)
	$(CC) $(CFLAGS) -MM $(SRCS) > .dep

# append the contents of .dep after this line.

include $(OBJS:.o=.d)

#_emalloc.o : _emalloc.c defs.h externs.h assert.h align.h globals.h globrename.h \
#  trace.h 
#_malloc.o : _malloc.c defs.h externs.h assert.h align.h globals.h globrename.h \
#  trace.h 
#_memalign.o : _memalign.c defs.h externs.h assert.h align.h globals.h globrename.h \
#  trace.h 
#_strdup.o : _strdup.c defs.h externs.h assert.h align.h globals.h globrename.h \
#  trace.h 
#_strsave.o : _strsave.c defs.h externs.h assert.h align.h globals.h globrename.h \
#  trace.h 
#botch.o : botch.c defs.h externs.h assert.h align.h globals.h globrename.h 
#dumpheap.o : dumpheap.c defs.h externs.h assert.h align.h globals.h globrename.h 
#emalloc.o : emalloc.c defs.h externs.h assert.h align.h globals.h globrename.h 
#getmem.o : getmem.c defs.h externs.h assert.h align.h globals.h globrename.h 
#leak.o : leak.c defs.h externs.h assert.h align.h globals.h globrename.h ./splay/sptree.h 
#malloc.o : malloc.c defs.h externs.h assert.h align.h globals.c globrename.h \
#  version.c 
#memalign.o : memalign.c defs.h externs.h assert.h align.h globals.h globrename.h 
#setopts.o : setopts.c defs.h externs.h assert.h align.h globals.h globrename.h 
#stats.o : stats.c defs.h externs.h assert.h align.h globals.h globrename.h 
#strdup.o : strdup.c defs.h externs.h assert.h align.h 
#strsave.o : strsave.c defs.h externs.h assert.h align.h 
#verify.o : verify.c defs.h externs.h assert.h align.h globals.h globrename.h 
