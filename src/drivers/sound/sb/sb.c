#include "sound.h" /* DEBUG(X), among others */
#include <kernel/memory.h>

#define vpokeb(O,V)		i386_lpoke8(0xB8000 + (O), V)

/* DSP buffers -- large for few interrupts, small for responsiveness */
#define	NUM_BUFS		2
#define	LG2_BUF_SIZE	13

/* BUF_SIZE must be a multiple of PAGE_SIZE => LG2_BUF_SIZE >= PAGE_BITS */
#define	BUF_SIZE		(1u << (LG2_BUF_SIZE))

#define	PEEK_DLY		100
#define	POKE_DLY		100
#define	RESET_DLY		100

#define	SBREG_MIX_ADR	0x04
#define	SBREG_MIX_DATA	0x05
#define	SBREG_RESET		0x06
#define	SBREG_READ		0x0A
#define	SBREG_WRITE		0x0C
#define	SBREG_POLL		0x0E

#define	SBCMD_PLAY		0x14	/* 8-bit DMA low-speed playback */
#define	SBCMD_SET_RATE	0x40	/* set sampling rate */
#define	SBCMD_SPKR_ON	0xD1	/* turn on speaker */
#define	SBCMD_SPKR_OFF	0xD3	/* turn OFF speaker */
#define	SBCMD_VERSION	0xE1	/* get DSP version */
#define	SBCMD_INTERRUPT	0xF2	/* generate an interrupts (for IRQ probe) */

/* in DMA.C */
int dma_from_mem(unsigned char chan, unsigned long adr, unsigned long count,
		bool auto_init);

static unsigned char * volatile _dsp_buff_out;
static unsigned char *_dsp_buff_base, *_dsp_buff_in;
static volatile bool _playback_in_progress;

static unsigned short _ioadr;
static unsigned char _irq, _dma = 3;
static bool _is_open, _16bit, _stereo;

typedef struct sb_t sb_t;
struct sb_t
{
	sound_t snd;
	device_t *dev;
};

/*****************************************************************************
	name:	sb_poke
	action:	waits up to 0.1 second for Sound Blaster DSP to become ready,
		then writes command byte val
	returns:0  if success
		-1 if timeout
*****************************************************************************/
static int sb_poke(sound_t *snd, unsigned char val)
{
	unsigned temp;

	for(temp = POKE_DLY; temp != 0; temp--)
	{
		if ((in(_ioadr + SBREG_WRITE) & 0x80) == 0)
		{
			out(_ioadr + SBREG_WRITE, val);
			return 0;
		}
		msleep(1);
	}
	return -1;
}
/*****************************************************************************
	name:	sb_peek
	action:	waits up to 0.1 second for Sound Blaster DSP to become ready,
		then reads data byte from chip
	returns:0  if success
		-1 if timeout
*****************************************************************************/
static int sb_peek(sound_t *snd)//unsigned long Timeout)
{
	unsigned temp;

	for(temp = PEEK_DLY; temp != 0; temp--)
	{
		if((in(_ioadr + SBREG_POLL) & 0x80) == 0)
			return in(_ioadr + SBREG_READ);
		msleep(1);
	}
	return -1;
}
/*****************************************************************************
*****************************************************************************/
static int sb_detect(sound_t *snd, unsigned short ioadr)
{
	unsigned short temp;

/* SB reset:
   1) Write a 1 to the reset port (2x6) */
	out(ioadr + SBREG_RESET, 0x01);
/* 2) Wait for 3 microseconds */
	msleep(1);
/* 3) Write a 0 to the reset port (2x6) */
	out(ioadr + SBREG_RESET, 0);
/* 4) Poll the read-buffer status port (2xE) until bit 7 is set */
	for(temp = RESET_DLY; temp != 0; temp--)
	{
		msleep(1);
		if(in(ioadr + SBREG_POLL) & 0x80)
			break;
	}
	if(temp == 0)
		return -1;
/* 5) Poll the read data port (2xA) until you receive an AA

The DSP usually takes about 100 microseconds to initialized itself.
After this period of time, if the return value is not AA or there
is no data at all, then the SB card may not be installed or an
incorrect I/O address is being used. */
	for(temp = RESET_DLY; temp != 0; temp--)
	{
		msleep(1);
		if(in(ioadr + SBREG_READ) == 0xAA)
			break;
	}
	if(temp == 0)
		return -1;

	(void)sb_poke(snd, SBCMD_VERSION);
	//temp = sb_peek(snd);
	temp = in(_ioadr + SBREG_READ);
	wprintf(L"\tDSP version: %u.%u\n", temp, in(_ioadr + SBREG_READ));

	return 0;
}
/*****************************************************************************
*****************************************************************************/
static void sb_set_volume(sound_t *snd, unsigned char level)
{
// xxx
}
/*****************************************************************************
*****************************************************************************/
static long sb_find_closest_rate(sound_t *snd, long rate)
{
	unsigned short time_const;
	unsigned long ret_val;

/* xxx - different algorithm for high-speed */
	time_const = 256L - 1000000L / rate;
	if(time_const > 255)
		return 0;
	ret_val = 1000000L / (256L - time_const);
	TRACE2("sb_find_closest_rate: %ld -> %ld\n", rate, ret_val);
	return ret_val;
}
/*****************************************************************************
*****************************************************************************/
static int sb_set_fmt(sound_t *snd, unsigned char depth, 
					  unsigned char channels, long rate)
{
	unsigned short time_const;
	unsigned char temp;

/* xxx - different algorithm for high-speed */
	time_const = 256L - 1000000L / rate;
	if(time_const > 255)
		return -1;
	if(sb_poke(snd, SBCMD_SET_RATE) != 0 || sb_poke(snd, time_const) != 0)
		return -1;

/* need SB16 (or better) only */
//	if(_Major < 0/* xxx */)
//		return -1;
//	if(_Minor < 0/* xxx */)
//		return -1;
	if(depth == 16)
		_16bit = true;
//		_Mode |= 0x0100;
	else if(depth == 8)
		_16bit = false;
//		_Mode &= ~0x0100;

/* need SB Pro (or better) only */
//	if(_Major < 0/* xxx */)
//		return -1;
//	if(_Minor < 0/* xxx */)
//		return -1;

	out(_ioadr + SBREG_MIX_ADR, 0x0E);
	temp = in(_ioadr + SBREG_MIX_DATA);
	if(channels == 2)
	{
		_stereo = true;
		temp |= 2;
	}
	else
	{
		_stereo = false;
		temp &= ~2;
	}
	out(_ioadr + SBREG_MIX_DATA, temp);
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static void sb_start_playback(sound_t *snd, unsigned long count)
{
	count--;
/* unmute outputs, if not already unmuted */
	(void)sb_poke(snd, SBCMD_SPKR_ON);
/* start playback
xxx - different cmd (0x91 ?) for high-sped */
	(void)sb_poke(snd, SBCMD_PLAY);
	(void)sb_poke(snd, count);
	(void)sb_poke(snd, count >> 8);
	_playback_in_progress = true;
}
/*****************************************************************************
*****************************************************************************/
static void sb_stop_playback(sound_t *snd, bool wait)
{
	if(wait)
	{
		TRACE0("waiting...\n");
		while(_playback_in_progress)
			/* nothing */;
	}
	else
	{
/* mute outputs */
		(void)sb_poke(snd, SBCMD_SPKR_OFF);
/* end playback */
		// xxx
		_playback_in_progress = false;
	}
}
/*****************************************************************************
*****************************************************************************/
static void sb_irq(sound_t *snd, unsigned num)
{
	unsigned short in_buf, out_buf;

	TRACE0("sb_irq\n");

/* clear interrupt at Sound Blaster */
	(void)in(_ioadr + SBREG_POLL);
/* clear interrupt at 8259 interrupt chips */
	//out(0x20, 0x20);
	//if(_irq >= 8)
		//out(0xA0, 0x20);
/* advance pointer */
	_dsp_buff_out += BUF_SIZE;
	if(_dsp_buff_out >= _dsp_buff_base + NUM_BUFS * BUF_SIZE)
		_dsp_buff_out = _dsp_buff_base;
/* are we now trying to play from the buffer currently being written? */
	in_buf = _dsp_buff_in - _dsp_buff_base;
	in_buf >>= LG2_BUF_SIZE;
	out_buf = _dsp_buff_out - _dsp_buff_base;
	out_buf >>= LG2_BUF_SIZE;
/* yes, stop playback */
	if(in_buf == out_buf)
	{
/* do NOT use printf() in an interrupt handler! */
		TRACE0("sb_irq: stopping\n");
		sb_stop_playback(snd, false);
	}
}
/*****************************************************************************
*****************************************************************************/
static int write_sample(sound_t *snd, short left, short right)
{
	unsigned short in_buf, out_buf, new_in_buf, count;
	bool wait = false;

/* to which buffer are we writing? */
	in_buf = _dsp_buff_in - _dsp_buff_base;
	in_buf >>= LG2_BUF_SIZE;
	while(1)
/* from which buffer are we playing? */
	{
		out_buf = _dsp_buff_out - _dsp_buff_base;
		out_buf >>= LG2_BUF_SIZE;
/* if they are the same, and if playback is active,
then wait until playback of this buffer is complete */
		if(out_buf != in_buf)
			break;
		if(!_playback_in_progress)
			break;
		if(!wait)
		{
			TRACE0("waiting...");
			wait = true;
		}
	}
/* store sample and advance pointer */
	if(_16bit)
	{
		write_le16(_dsp_buff_in, left);
		_dsp_buff_in += 2;
		if(_stereo)
		{
			write_le16(_dsp_buff_in, right);
			_dsp_buff_in += 2;
			count = BUF_SIZE >> 2;
		}
		else
			count = BUF_SIZE >> 1;
	}
	else
	{
		left >>= 8;
		*_dsp_buff_in = left;
		_dsp_buff_in++;
		if(_stereo)
		{
			right >>= 8;
			*_dsp_buff_in = right;
			_dsp_buff_in++;
			count = BUF_SIZE >> 1;
		}
		else
			count = BUF_SIZE;
	}
	if(_dsp_buff_in >= _dsp_buff_base + NUM_BUFS * BUF_SIZE)
		_dsp_buff_in = _dsp_buff_base;
/* did we cross over into a new buffer? */
	new_in_buf = _dsp_buff_in - _dsp_buff_base;
	new_in_buf >>= LG2_BUF_SIZE;
/* if yes, and if not currently playing, then kick-start playback */
	if((new_in_buf != in_buf) && !_playback_in_progress)
	{
/* you should see this message ONCE, when playback starts
see it again if you pause/unpause the playback

if you see it many times, something's wrong */
		wprintf(L"*** kicking playback ***\n");
		sb_start_playback(snd, count);
	}
	return 0;
}
/*****************************************************************************
*****************************************************************************/

#define	f2l(X)	((addr_t)(X))

/*****************************************************************************
*****************************************************************************/
static void sb_close(sound_t *snd)
{
	sb_t *sb = (sb_t*) snd;
	TRACE0("sb_close\n");
/* already closed? */
	if(!_is_open)
	{
		wprintf(L"sb_close: sound hardware already closed\n");
		return;
	}
/* abort playback */
	if(_playback_in_progress)
	{
		TRACE0("aborting playback...\n");
		sb_stop_playback(snd, false);
	}
/* restore old interrupt vector */
	devRegisterIrq(sb->dev, _irq, false);
	_is_open = false;
}

/*****************************************************************************
*****************************************************************************/
static bool sb_open(sound_t *snd)
{
	static const unsigned short base_io_adr[] =
	{
		0x220, 0x230, 0x240
	};
	unsigned short temp;
//	unsigned char dma_mask;
	int err = -1;
	dword index;
	sb_t *sb = (sb_t*) snd;

/* already open? */
	if(_is_open)
	{
		wprintf(L"sb_open: sound hardware already open\n");
		return false;
	}

	index = devFindResource(sb->dev->config, dresIrq, 0);
	if (index == (dword) -1)
		return false;

	_irq = sb->dev->config->resources[index].u.irq;

/* PROBE FOR HARDWARE */
	TRACE1("sb_open: probing for Sound Blaster: IRQ %d\n", _irq);
/* probe for DSP chip */
	temp = 0;
	for(; temp < sizeof(base_io_adr) / sizeof(base_io_adr[0]); temp++)
	{
		_ioadr = base_io_adr[temp];
		TRACE1("\tprobing at I/O=0x%03X...\n", _ioadr);
		err = sb_detect(snd, _ioadr);
		if(err == 0)
			break;
	}
	if(err != 0)
	{
		wprintf(L"\terror: Sound Blaster not detected\n");
		return false;
	}

	for (temp = 3; temp != 0; temp--)
	{
/* trigger IRQ */
		(void)sb_poke(snd, SBCMD_INTERRUPT);
/* clear the generated IRQ */
		(void)in(_ioadr + SBREG_POLL);
	}

/* clear dangling IRQ at DSP chip */
//	(void)in(_ioadr + SBREG_POLL);
/* enable interrupts at 8259 interrupt controller chips */
	temp = 1;
	temp <<= _irq;
	out(0x21, in(0x21) & ~temp);
	temp >>= 8;
	out(0xA1, in(0xA1) & ~temp);
/* allocate low memory for DSP buffers - xxx
_dsp_buff_mem = (unsigned char *)farmalloc(65536L + NUM_BUFS * BUF_SIZE);
_dsp_buff_base = align_to_64k(_dsp_buff_mem);
	then farfree(_dsp_buff_mem) in sb_close()
or use INT 21h AH=48h instead of farmalloc() */

	/* xxx - fix this */
	_dsp_buff_base = 
		(unsigned char*) memAllocLowSpan((BUF_SIZE * NUM_BUFS) / PAGE_SIZE);
	if (_dsp_buff_base == NULL)
	{
		wprintf(L"\tFailed to allocate %u pages\n", (BUF_SIZE * NUM_BUFS) / PAGE_SIZE);
		return false;
	}

	_dsp_buff_in = _dsp_buff_base;
	_dsp_buff_out = _dsp_buff_base;
/* */
	wprintf(L"\tSound Blaster at I/O=0x%03X, IRQ=%u, DMA=%u\n",
		_ioadr, _irq, _dma);
	wprintf(L"\t%u DSP buffers at %p (%u bytes each, %u bytes total)\n",
		NUM_BUFS, _dsp_buff_base, BUF_SIZE, NUM_BUFS * BUF_SIZE);
/* arm DMA */
	err = dma_from_mem(_dma, f2l(_dsp_buff_base),
//		NUM_BUFS * BUF_SIZE, true);
		BUF_SIZE, false);
	if(err != 0)
		return false;
/* crap, don't forget this, or sb_close() won't work */
	_is_open = true;
/* take over interrupt vector */
	devRegisterIrq(sb->dev, _irq, true);
	return true;
}

/*****************************************************************************
*****************************************************************************/
sb_t sb_drv =
{
	{
		sb_open,
		sb_find_closest_rate,
		sb_set_fmt,
		sb_set_volume,
		write_sample,
		sb_stop_playback,
		sb_close,
		sb_irq,
	},
	NULL
};


sound_t *sb_init(device_t *dev)
{
	sb_drv.dev = dev;
	return &sb_drv.snd;
}