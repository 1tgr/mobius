# Generated automatically from Makefile.in by configure.
# $Id: Makefile,v 1.3 2002/08/17 22:52:13 pavlovskii Exp $
#
# Auto-configuring Makefile for the Netwide Assembler.
#
# The Netwide Assembler is copyright (C) 1996 Simon Tatham and
# Julian Hall. All rights reserved. The software is
# redistributable under the licence given in the file "Licence"
# distributed in the NASM archive.

include ../make.conf

srcdir		= .
prefix		= /cygdrive/f
exec_prefix	= ${prefix}
bindir		= ${exec_prefix}/bin
mandir		= ${prefix}/man

CC		= gcc
CFLAGS		= -g -O2 -Wall -ansi -pedantic -I$(srcdir) -I. -I/mn/include
LDFLAGS		= $(LIB)/libc.lib $(LIB)/libsys.lib

INSTALL		= /usr/bin/install -c
INSTALL_PROGRAM	= ${INSTALL}
INSTALL_DATA	= ${INSTALL} -m 644

NROFF		= nroff

.SUFFIXES: .c .i .s .o .1 .man

.PHONY: all doc rdf install clean distclean cleaner spotless install_rdf
.PHONY: install_doc everything install_everything

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.c.s:
	$(CC) -S $(CFLAGS) -o $@ $<

.c.i:
	$(CC) -E $(CFLAGS) -o $@ $<

.1.man:
	$(NROFF) -man $< > $@

NASM = nasm.o nasmlib.o float.o insnsa.o assemble.o labels.o \
       parser.o outform.o outbin.o outaout.o outcoff.o outelf.o \
       outobj.o outas86.o outrdf.o outrdf2.o outdbg.o zoutieee.o \
       preproc.o listing.o eval.o

NDISASM = ndisasm.o disasm.o sync.o nasmlib.o insnsd.o

all: $(BIN)/nasm.exe $(BIN)/ndisasm.exe nasm.man ndisasm.man

$(BIN)/nasm.exe: $(NASM)
	ld -o $(BIN)/nasm.exe $(NASM) $(LIB)/crt0.o $(LDFLAGS) 

$(BIN)/ndisasm.exe: $(NDISASM)
	ld -o $(BIN)/ndisasm.exe $(NDISASM) $(LIB)/crt0.o $(LDFLAGS) 

# These source files are automagically generated from a single
# instruction-table file by a Perl script. They're distributed,
# though, so it isn't necessary to have Perl just to recompile NASM
# from the distribution.

insnsa.c: insns.dat insns.pl
	perl $(srcdir)/insns.pl -a $(srcdir)/insns.dat
insnsd.c: insns.dat insns.pl
	perl $(srcdir)/insns.pl -d $(srcdir)/insns.dat
insnsi.h: insns.dat insns.pl
	perl $(srcdir)/insns.pl -i $(srcdir)/insns.dat
insnsn.c: insns.dat insns.pl
	perl $(srcdir)/insns.pl -n $(srcdir)/insns.dat

# This source file is generated from the standard macros file
# `standard.mac' by another Perl script. Again, it's part of the
# standard distribution.

macros.c: standard.mac macros.pl
	perl $(srcdir)/macros.pl $(srcdir)/standard.mac

install: nasm ndisasm
	$(INSTALL_PROGRAM) nasm $(INSTALLROOT)$(bindir)/nasm
	$(INSTALL_PROGRAM) ndisasm $(INSTALLROOT)$(bindir)/ndisasm
	$(INSTALL_DATA) $(srcdir)/nasm.1 $(INSTALLROOT)$(mandir)/man1/nasm.1
	$(INSTALL_DATA) $(srcdir)/ndisasm.1 $(INSTALLROOT)$(mandir)/man1/ndisasm.1

clean:
	rm -f *.o *.s *.i nasm ndisasm
	cd rdoff && $(MAKE) clean

distclean: clean
	rm -f config.* Makefile *~ *.bak *.lst *.bin
	cd rdoff && $(MAKE) distclean

cleaner: clean
	rm -f insnsa.c insnsd.c insnsi.h insnsn.c macros.c *.man
	cd doc && $(MAKE) clean

spotless: distclean cleaner

rdf:
	cd rdoff && $(MAKE)

rdf_install install_rdf:
	cd rdoff && $(MAKE) install

doc:
	cd doc && $(MAKE) all

doc_install install_doc:
	cd doc && $(MAKE) install

everything: all doc rdf

install_everything: everything install install_doc install_rdf

include $(NASM:.o=.d)
include $(NDISASM:.o=.d)
