PREFIX			= /usr
BINDIR			= $(PREFIX)/games
MANDIR			= $(PREFIX)/man/man6
MANSUFFIX		= 6
MKDIR			= mkdir
INSTALL			= install
INSTALLFLAGS	= -c
INSTBINFLAGS	= $(INSTALLFLAGS) -s -m 0755
INSTMANFLAGS	= $(INSTALLFLAGS) -m 0644

LN			= ln

# This isn't used at the moment.
GZIP		= gzip
GZIPFLAGS	= -9

CC			= gcc
PROF		= -g

I586_L_FLAGS	=
I586_C_FLAGS	= -Wall $(PROF) -I.

C_FLAGS	= -O3 $(I586_C_FLAGS)
L_FLAGS	= -O3 $(I586_L_FLAGS)
L_POSTFLAGS	=

PROGNAME = gfortune

# ----------------------------------------------------------------------
# As of here you can keep your meddling hands out of my makefile.
# ----------------------------------------------------------------------






# ----------------------------------------------------------------------
# Go on, scram. Scat.
# ----------------------------------------------------------------------



# ----------------------------------------------------------------------
# Don't say I didn't warn you.
# ----------------------------------------------------------------------

O_FILES = gfortune.o

$(PROGNAME): $(O_FILES)
	rm -f $(PROGNAME)
	$(CC) $(C_FLAGS) $(L_FLAGS) -o $(PROGNAME) $(O_FILES) $(L_POSTFLAGS)

.c.o:
	$(CC) -c $(C_FLAGS) $<

install:: $(PROGNAME)
	$(MKDIR) -p $(BINDIR)
	$(INSTALL) $(INSTBINFLAGS) $(PROGNAME) $(BINDIR)/$(PROGNAME)
	@if [ ! -e $(BINDIR)/fortune ]; then					\
		$(LN) -s $(BINDIR)/$(PROGNAME) $(BINDIR)/fortune;	\
	else													\
		@echo "Not symlinking to 'fortune' - program exists"\
	fi

install.man:: $(PROGNAME).man
	$(MKDIR) -p $(MANDIR)
	$(INSTALL) $(INSTMANFLAGS) $(PROGNAME).man \
		$(MANDIR)/$(PROGNAME).$(MANSUFFIX)
#	$(GZIP) $(GZIPFLAGS) $(MANDIR)/$(PROGNAME).$(MANSUFFIX)

clean: dummy
	rm -f core $(O_FILES) $(PROGNAME)

distclean: clean
	rm -f *~

dummy:      
