DIRS=	ata isa pci kdebug ps2mouse sermouse keyboard fdc fat vgatext

all:
	for %i in ($(DIRS)) do @make -C %i

clean:
	for %i in ($(DIRS)) do @make -C %i clean
