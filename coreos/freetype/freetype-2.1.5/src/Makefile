ROOT=	../../../..

TARGET_D=	$(BIN)/freetype.dll
TARGET_S=	$(LIB)/freetype_s.lib
IMP=	$(LIB)/freetype.lib
DEF=	freetype.def
BASE=	freetype
DEFINES=	-I../include
LIBS=	$(LIB)/libc.lib $(LIB)/libsys.lib

AUTOHINT=	autohint/autohint.o
FT_BASE=	base/ftbase.o base/ftglyph.o base/ftsystem.o base/ftinit.o
BDF=	bdf/bdf.o
CACHE=	cache/ftcache.o
CFF=	cff/cff.o
CID=	cid/type1cid.o
GZIP=	gzip/ftgzip.o
OTLAYOUT= otlayout/otlbase.o otlayout/otlcommn.o otlayout/otlgdef.o otlayout/otlgpos.o \
	otlayout/otlgsub.o otlayout/otljstf.o otlayout/otlparse.o
PCF=	pcf/pcf.o
PSAUX=	psaux/psaux.o
PSHINTER=	pshinter/pshinter.o
PSNAMES=	psnames/psnames.o
RASTER=	raster/raster.o
SFNT=	sfnt/sfnt.o
SMOOTH=	 smooth/smooth.o
TRUETYPE= truetype/truetype.o
TYPE1=	type1/type1.o
TYPE42=	 type42/type42.o
WINFONTS=	winfonts/winfnt.o
	
#OBJS=	dllmain.o \
	$(AUTOHINT) \
	$(FT_BASE) \
	$(BDF) \
	$(CACHE) \
	$(CFF) \
	$(CID) \
	$(GZIP) \
	$(PCF) \
	$(PSAUX) \
	$(PSHINTER) \
	$(PSNAMES) \
	$(RASTER) \
	$(SFNT) \
	$(SMOOTH) \
	$(TRUETYPE) \
	$(TYPE1) \
	$(TYPE42) \
	$(WINFONTS)
	
OBJS=	dllmain.o \
	$(FT_BASE) \
	$(CACHE) \
	$(PSNAMES) \
	$(RASTER) \
	$(SFNT) \
	$(SMOOTH) \
	$(TRUETYPE) \
	$(WINFONTS)

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions
