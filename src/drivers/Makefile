DIRS=	ata fat tty fdc keyboard cmos video pci ps2mouse

all:
	for dir in $(DIRS); do make -C $$dir; done

clean:
	for dir in $(DIRS); do make -C $$dir clean; done
