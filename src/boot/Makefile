include ../make.actions

LOADER=	$(BIN)/bootsect.com
LOADER2=$(BIN)/mobel_pe.com

all:	$(LOADER) $(LOADER2)

$(BIN)/%.com:	%.asm
	nasm -f bin -o $@ $<

clean:
	rm $(LOADER)
	rm $(LOADER2)
	rm mobel.d bootsect.d

include mobel.d
include bootsect.d