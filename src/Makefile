DIRS=	librtl libsys libc kernel/i386 kernel drivers test shell boot ../bin

all:
	for dir in $(DIRS); do make -C $$dir; done

clean:
	for dir in $(DIRS); do make -C $$dir clean; done

floppy:	all
	make -C ../bin floppy