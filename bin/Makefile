FILES1=	mobel_pe.com test.exe test.txt libsys.dll \
	kernel.exe kernel.cfg ata.drv 
FILES2=	fat.drv tty.drv keyboard.drv fdc.drv shell.exe

all:
	mcopy.cmd -o $(FILES1) a:
	mcopy.cmd -o $(FILES2) a:

#floppy:
#	for f in $(FILES1) $(FILES2); do \
#		echo $$f ;\
#		cp $$f /cygdrive/a/; \
#		done

floppy:	all
	raw32
