DIRS=	libsys libc kernel \
	libgui libposix freetype \
	framebuf\ CONFIG=kernel framebuf\ CONFIG=user \
	drivers apps console desktop monitor \
	gconsole winmgr

all:
	for dir in $(DIRS); do make -C $$dir; done

clean:
	for dir in $(DIRS); do make -C $$dir clean; done

floppy:	all
	make -C ../distrib floppy
