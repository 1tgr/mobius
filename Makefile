DIRS=	librtl libsys libc \
	kernel drivers test shell \
	hello nasm vidtest \
	console ../distrib

all:
	for dir in $(DIRS); do make -C $$dir; done

clean:
	for dir in $(DIRS); do make -C $$dir clean; done

floppy:	all
	make -C ../distrib floppy
