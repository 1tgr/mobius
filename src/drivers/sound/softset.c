/*****************************************************************************
softset IRQ and DMA of original Windows Sound System board
Chris Giese		http://www.execpc.com/~geezer
*****************************************************************************/
#include <stdlib.h> /* atoi() */
#include <stdio.h> /* printf() */
#include <dos.h> /* union REGS, int86(), inportb(), outportb() */

#if 0
#define	DEBUG(X)	X
#else
#define	DEBUG(X)
#endif

#define	POWER_UP_DLY	100000L
/*****************************************************************************
*****************************************************************************/
static int ad1848_detect(unsigned short ioadr)
{
	unsigned char temp, reg_val;
	unsigned long timeout;

/* poll index register of AD1848 until it returns something other than 0x80 */
	for(timeout = POWER_UP_DLY; timeout != 0; timeout--)
	{
		if(inportb(ioadr) != 0x80)
			break;
	}
	if(timeout == 0)
		return -1;
	DEBUG(
	timeout = POWER_UP_DLY - timeout;
	if(timeout != 0)
		printf("<waited %lu clocks>\n", timeout);
	)
/* try writing various AD1848 registers (left and right input gain
and control regs) to see if chip is there */
	for(temp = 0; temp < 2; temp++)
	{
		outportb(ioadr + 0, temp);
		outportb(ioadr + 1, 0xAA);
		reg_val = inportb(ioadr + 1);
		outportb(ioadr + 1, 0x45);/* b4 is reserved; always write 0 */
		if(reg_val != 0xAA || inportb(ioadr + 1) != 0x45)
			return -1;
	}
/* try changing the chip revision ID bits (they are read-only) */
	outportb(ioadr + 0, 0x0C);
	reg_val = inportb(ioadr + 1);
	outportb(ioadr + 1, reg_val ^ 0x0F);
	if(((inportb(ioadr + 1) ^ reg_val) & 0x0F) != 0)
		return -1;
/* found it! */
	return 0;
}
/*****************************************************************************
from PORTS.B of Ralf Brown's Interrupt List
(this is for WSS emulation by OPTi "Vendetta" chipset,
but it seems to work my original WSS board)

Bit(s)	Description	(Table P0898)
 7	reserved
 6	IRQ sense source
	0 = normal
	1 = interrupt auto-selection
 5-3	WSS IRQ
	000 = disable
	001 = IRQ7
	010-100 = IRQ9-IRQ11
	101 = IRQ5
	110-111 = reserved
 2-0	WSS DRQ
	      playback	capture
	000 = disable	disable
	001 = DRQ0	disable
	010 = DRQ1	disable
	011 = DRQ3	disable
	100 = disable	DRQ1
	101 = DRQ0	DRQ1
	110 = DRQ1	DRQ0
	111 = DRQ3	DRQ0
*****************************************************************************/
int main(int arg_c, char *arg_v[])
{
	static const unsigned short base_io_adr[] =
	{
		0x530, 0x604, 0xE80, 0xF40
	};
	unsigned char irq, dma, irq_bits, dma_bits, temp;
	unsigned short ioadr = 0x530;
	union REGS regs;
	int err;

	if(arg_c < 3)
	{
		printf("Softsets Windows Sound System IRQ and DMA. Usage:\n"
			"\t""softset irq dma\nirq=7,9,10,11; dma=0,1,3\n");
		return 1;
	}
/* get IRQ num from command line and validate it */
	irq = atoi(arg_v[1]);
	irq_bits = 0;
	if(irq == 7)
		irq_bits |= 0x08;
	else if(irq == 9)
		irq_bits |= 0x10;
	else if(irq == 10)
		irq_bits |= 0x18;
	else if(irq == 11)
		irq_bits |= 0x20;
/* maybe IRQ 5 for OPTi Vendetta chip, but not for original WSS...
	else if(irq == 5)
		irq_bits |= 0x28; */
	else
	{
		printf("Illegal IRQ number %u\n", irq);
		return 2;
	}
/* get DMA num from command line and validate it */
	dma = atoi(arg_v[2]);
/* NOTE: capture (record) DMA is always disabled here, either change
this or use single-mode DMA (set SDC bit in reg 9 of AD1848) */
	dma_bits = 0;
	if(dma == 0)
		dma_bits |= 0x01;
	else if(dma == 1)
		dma_bits |= 0x02;
	else if(dma == 3)
		dma_bits |= 0x03;
	else
	{
		printf("Illegal DMA number %u\n", dma);
		return 3;
	}
/* make sure it's really DOS */
	regs.x.ax=0x1600;
	int86(0x2F, &regs, &regs);
	if(regs.h.al != 0 && regs.h.al != 0x80)
	{
		printf("Detected Windows version ");
		if(regs.h.al == 0x01 || regs.h.al == 0xFF)
			printf("2.x");
		else
			printf("%u.%u", regs.h.al, regs.h.ah);
		printf(", aborting\n");
		return 4;
	}
/* probe for board
xxx - doesn't work! */
#if 1
ioadr = 0x530;
#else
	temp = 0;
	for(; temp < sizeof(base_io_adr) / sizeof(base_io_adr[0]); temp++)
	{
		ioadr = base_io_adr[temp] + 4;
		DEBUG(printf("\t""probing at I/O=0x%03X...\n", ioadr);)
		err = ad1848_detect(ioadr);
		if(err == 0)
			break;
	}
	if(err != 0)
	{
		printf("\t""error: chip not found\n");
		return 5;
	}
#endif
/* do it */
	outportb(ioadr + 0, irq_bits | 0x40);
	if((inportb(ioadr + 3) & 0x40) == 0)
		printf("IRQ conflict?\n");
	outportb(ioadr, irq_bits | dma_bits);
	return 0;
}
