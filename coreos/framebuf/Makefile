ROOT=	../..

ifeq ($(CONFIG),)
CONFIG=	kernel
endif

ifneq ($(CONFIG),kernel)
ifneq ($(CONFIG),user)
error Please define CONFIG as either user or kernel
endif
endif

TARGET_S=	$(LIB)/framebuf_$(CONFIG).lib
BASE=	framebuf_$(CONFIG)
OBJDIR=	$(CONFIG)
CFLAGS=	-I$(ROOT)/coreos/freetype/freetype-2.1.5/include

ifeq ($(CONFIG),kernel)
DEFINES=	-DKERNEL
endif

OBJS=	dllmain.o surface.o line.o convert.o \
	font.o accel.o bpp4.o bpp8.o

include $(ROOT)/coreos/make.conf
include $(ROOT)/coreos/make.actions
