DIRS=	librtl libsys libc kernel/i386 kernel drivers test console boot ../bin

all:
	for dir in $(DIRS); do make -C $$dir; done

clean:
	for dir in $(DIRS); do make -C $$dir clean; done