ROOT=	../..
TARGET=	$(BIN)/winmgr.exe

GFX=	gfx/clip.o gfx/cursor.o gfx/draw.o gfx/gfxbase.o \
		gfx/gfxmgr.o gfx/rect.o gfx/surface.o
		
OBJS=	server.o winmgr.o \
	$(GFX)
LIBS=	../libgui/objects/ipc.o \
	$(LIB)/crt0.o \
	$(LIB)/framebuf_user.lib \
	$(LIB)/freetype.lib \
	$(LIB)/libc.lib \
	$(LIB)/libsys.lib

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions
