#include <stdio.h> /* printf() */
#include "sound.h" /* bool */

static const unsigned short _single_reg[8] =
{
	0x0A, 0x0A, 0x0A, 0x0A, 0xD4, 0xD4, 0xD4, 0xD4
};
static const unsigned char _disable_cmd[8] =
{
	0x04, 0x05, 0x06, 0x07, 0x04, 0x05, 0x06, 0x07,
};
static const unsigned short _mode_reg[8] =
{
	0x0B, 0x0B, 0x0B, 0x0B, 0xD6, 0xD6, 0xD6, 0xD6
};
static const unsigned short _ff_reg[8] =
{
	0x0C, 0x0C, 0x0C, 0x0C, 0xD8, 0xD8, 0xD8, 0xD8
};
static const unsigned short _adr_reg[8] =
{
	0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC
};
static const unsigned short _page_reg[8] =
{
	0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A
};
static const unsigned short _count_reg[8] =
{
	0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE
};
static const unsigned char _enable_cmd[8] =
{
	0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,
};
/*****************************************************************************
*****************************************************************************/
int dma_from_mem(unsigned char chan, unsigned long adr, unsigned long count,
		bool auto_init)
{
	static const unsigned char write_cmd[8] =
	{
		0x48, 0x49, 0x4A, 0x4B, 0x48, 0x49, 0x4A, 0x4B
	};

/* validate chan */
	if(chan > 7)
	{
		wprintf(L"dma_from_mem: DMA channel (%u) > 7\n", chan);
		return -1;
	}
/* 16 bit data is halved */
	if(chan >= 4)
	{
		adr >>= 1;
		count >>= 1;
	}
	count--;
/* make sure transfer doesn't exceed max or cross a 64K boundary */
	if(adr + count >=  0x100000L) /* real mode; 1 meg */
//	if(adr + count >=  0x1000000L) /* pmode; 16 meg */
	{
		wprintf(L"dma_from_mem: end adr (0x%lX) exceeds max\n",
			adr + count);
		return -2;
	}
	if((adr & 0x10000L) != ((adr + count) & 0x10000L))
	{
		wprintf(L"dma_from_mem: DMA from adrs 0x%lX - 0x%lX "
			"crosses 64K boundary\n", adr, adr + count);
		return -3;
	}
/* disable channel */
	out(_single_reg[chan], _disable_cmd[chan]);
/* set mode */
	out(_mode_reg[chan],
		auto_init ? (write_cmd[chan] | 0x10) : write_cmd[chan]);
/* clear flip-flop, for LSB of address */
	out(_ff_reg[chan], 0);
/* address LSB */
	out(_adr_reg[chan], adr);
	adr >>= 8;
/* address MSB */
	out(_adr_reg[chan], adr);
	adr >>= 8;
/* page */
	out(_page_reg[chan], adr);
/* clear flip-flop, for LSB of count */
	out(_ff_reg[chan], 0);
/* count LSB */
	out(_count_reg[chan], count);
	count >>= 8;
/* count MSB */
	out(_count_reg[chan], count);
/* enable channel */
	out(_single_reg[chan], _enable_cmd[chan]);
	return 0;
}
