EXP=    libsys.exp
DEF=    libsys.def
OBJS=	syscall.o port.o misc.o resource.o
BASE=	libsys

include ../make.actions

TARGET_D=       $(BIN)/libsys.dll
TARGET_S=       $(LIB)/libsys_s.lib
IMP=            $(LIB)/libsys.lib
LIBS=           $(LIB)/libc_s.lib

#include ../make.actions

all:	$(TARGET_D) $(TARGET_S) $(IMP) sysdef.i

$(IMP):	$(DEF)
	dlltool -d $< -l $@ -D $(<:.def=.dll)

$(TARGET_S):	$(OBJS)
	ar rcs $@ $(OBJS)

$(TARGET_D):	 dllmain.o $(TARGET_S) $(EXP) $(LIBS) .base
	$(LD) --dll -o $@ --entry _DllMainCRTStartup \
		--image-base `cat .base` \
		dllmain.o $(EXP) $(TARGET_S) $(LIBS)

sysdef.i:	sysdef.c
	gcc -E $< -o $@ -I$(INCLUDE)

include $(OBJS:.o=.d)
