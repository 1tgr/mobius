DEF=    mgl.def
OBJS=	dllmain.o rc.o rcdevice.o bpp8.o \
	metafile.o bitmap.o font.o fontmanager.o
BASE=	mgl
TARGET_D=       $(BIN)/mgl.dll
TARGET_S=       $(LIB)/mgl_s.lib
IMP=            $(LIB)/mgl.lib
LIBS=           $(LIB)/freetype_s.lib $(LIB)/libc.lib $(LIB)/libsys.lib
CFLAGS=	-O2 -Ifreetype2/include

NO_EXP=1

include ../make.conf
include ../make.actions

subdirs:
	make -C freetype2

%.exp:	%.def $(TARGET_S)
	dlltool --export-all-symbols -e $@ -D $(<:.def=.dll) $(TARGET_S)

$(IMP):	$(DEF:.def=.exp)
	dlltool --export-all-symbols -l $@ -D $(<:.exp=.dll) $(TARGET_S)

all: subdirs
	make -C test
	objdump -ldC $(TARGET_D) > listing.txt
	cp $(TARGET_D) mgl_d.dll
	strip $(TARGET_D)
