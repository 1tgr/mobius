DIRS=	ata cdfs cmos ext2 \
	fat fdc isapnp keyboard \
	ne2000 pci ps2mouse rtl8139 \
	sermouse sound sound/es1371 sound/sb \
	video video/s3 video/vesa video/vga

all:
	for dir in $(DIRS); do make -C $$dir; done

clean:
	for dir in $(DIRS); do make -C $$dir clean; done

base:
	for dir in $(DIRS); do make -C $$dir .base; done
