DEF=	gui.def
OBJS=	dllmain.o stubs.o ipc.o \
	window.o button.o frame.o \
	application.o view.o dialog.o edit.o \
	label.o control.o container.o messagebox.o
BASE=	gui
TARGET_D=	$(BIN)/gui.dll
TARGET_S=	$(LIB)/gui_s.lib
IMP=	$(LIB)/gui.lib
LIBS=	$(LIB)/mgl.lib $(LIB)/libc.lib $(LIB)/libsys.lib
CFLAGS=	-O2

#NO_EXP=1

include ../make.conf
include ../make.actions

#%.exp:	%.def $(TARGET_S)
#	dlltool --export-all-symbols -e $@ -D $(<:.def=.dll) $(TARGET_S)

#$(IMP):	$(DEF:.def=.exp)
#	dlltool --export-all-symbols -l $@ -D $(<:.exp=.dll) $(TARGET_S)

all:
	make -C desktop
	make -C gconsole
	make -C test
	make -C winmgr
