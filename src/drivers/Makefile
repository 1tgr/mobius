DIRS=	ata cmos fat fdc \
	isapnp iso9660 pci ps2mouse \
	video tty keyboard ext2 \
	sound ne2000 rtl8139

all:
	for dir in $(DIRS); do make -C $$dir; done

clean:
	for dir in $(DIRS); do make -C $$dir clean; done
