EXP=    libsys.exp
DEF=    libsys.def
OBJS=	syscall.o port.o misc.o
BASE=	0x6000000

include ../make.actions

TARGET_D=       $(BIN)/libsys.dll
TARGET_S=       $(LIB)/libsys_s.lib
IMP=            $(LIB)/libsys.lib
LIBS=           $(LIB)/libc_s.lib
include ../make.actions

all:	$(TARGET_D) $(TARGET_S) $(IMP)

$(IMP):	$(DEF)
	dlltool -d $< -l $@ -D $(<:.def=.dll)

$(TARGET_S):	$(OBJS)
	ar rcs $@ $(OBJS)

$(TARGET_D):	 dllmain.o $(TARGET_S) $(EXP) $(LIBS)
	$(LD) --dll -o $@ --entry _DllMainCRTStartup \
		--image-base $(BASE) \
		dllmain.o $(EXP) $(TARGET_S) $(LIBS)

include $(OBJS:.o=.d)
