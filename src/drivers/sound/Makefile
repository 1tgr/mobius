TARGET=	$(BIN)/sound.drv
BASE=	sound
LIBS=	$(LIB)/pci.lib

WSS=	wss/wss.o
SB=	sb/sb.o
SOLO1=	solo1/solo1.o
FORMATS=wav.o wav01.o wav02.o wav06.o wav07.o wav34.o	\
	wav49.o wav85.o au.o au01.o au023.o au27.o
OBJS=	sound.o dma.o $(SOLO1)
LIBS=	$(LIB)/pci.lib

include ../make.driver

include $(OBJS:.o=.d)
