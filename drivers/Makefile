DIRS=	ata cmos fat fdc \
	isapnp pci ps2mouse \
	video tty keyboard ext2 \
	sound ne2000 rtl8139 sermouse

all:
	for dir in $(DIRS); do make -C $$dir; done

clean:
	for dir in $(DIRS); do make -C $$dir clean; done

base:
	for dir in $(DIRS); do make -C $$dir .base; done
