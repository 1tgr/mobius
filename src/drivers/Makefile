DIRS=	ata cmos fat fdc \
	isapnp iso9660 pci ps2mouse \
	video tty keyboard

all:
	for dir in $(DIRS); do make -C $$dir; done

clean:
	for dir in $(DIRS); do make -C $$dir clean; done
