include make.actions

DIRS=	kernel drivers kernelu libc \
	freetype-2.0.4 mgl \
	devtest init short console \
	tetris hello boot ../bin

all:	crt0.o crt0dll.o
	for %i in ($(DIRS)) do @make -C %i

clean:
	for %i in ($(DIRS)) do @make -C %i clean

floppy:	all
	make -C ../bin floppy
