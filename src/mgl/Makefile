TARGET_D=	$(BIN)/mgl.dll
TARGET_S=	$(LIB)/mgl_s.lib
OBJS=	rc.o render.o rect.o wmf.o mgl.res.o
EXP=	mgl.exp
IMP=	$(LIB)/mgl.lib
CFLAGS=	-g
#LIBS=	$(LIB)/freetype_s.lib $(LIB)/libc.lib $(LIB)/kernelu.lib
LIBS=	$(LIB)/libc.lib $(LIB)/libsys.lib
BASE=	mgl

include ../make.actions

all:	$(TARGET_D) $(IMP)
	make -C test

$(IMP):	$(EXP:.exp=.def)
	dlltool -d $< -l $@ -D $(<:.def=.dll)

$(TARGET_S):	$(OBJS)
	ar rcs $@ $(OBJS)

$(TARGET_D):	 dllmain.o $(TARGET_S) $(EXP) $(LIBS) .base
	$(LD) --dll -o $@ --entry _DllMainCRTStartup \
		--image-base `cat .base` \
		dllmain.o $(EXP) $(TARGET_S) $(LIBS)

include $(OBJS:.o=.d)
