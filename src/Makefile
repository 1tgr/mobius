DIRS=	librtl libsys libc kernel/i386 \
	kernel drivers test shell \
	hello nasm vidtest mgl ../bin

all:
	make -C boot --win32
	for dir in $(DIRS); do make -C $$dir; done

clean:
	make -C boot --win32 clean
	for dir in $(DIRS); do make -C $$dir clean; done

floppy:	all
	make -C ../bin floppy
