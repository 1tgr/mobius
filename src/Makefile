include make.actions

DIRS=	kernel drivers kernelu libc devtest short console boot ../bin

all:	crt0.o crt0dll.o
	for %i in ($(DIRS)) do @make -C %i

clean:
	for %i in ($(DIRS)) do @make -C %i clean