/*****************************************************************************
driver for Windows Sound System (Analog Devices AD1848 chip)
xxx - the logic in ad1848_irq() that decides whether or not to
	call ad1848_stop_playback() may be defective

xxx - need to pad buffer with zeroes at end of playback
	that's why S8-PCM.WAV and M8-PCM.WAV are cut off at end
	(with large BUF_SIZE)

xxx - restore DMA-probing code

xxx - get softset (see SOFTSET.C) working properly
*****************************************************************************/
#include <stdio.h> /* printf() */
/* MK_FP(), FP_SEG(), FP_OFF(), setvect(), getvect() for Turbo C */
#include "sound.h" /* DEBUG(X), among others */
#include <kernel/memory.h>

/* various AD1848-related delays --
looks like a lot of thought went into them... */
#define	POWER_UP_DLY	100000L
#define	RESYNC_DLY		100000L
#define	ACI_HIGH_DLY	100000L
#define	ACI_LOW_DLY		100000L

/* DSP buffers -- large for few interrupts, small for responsiveness */
#define	NUM_BUFS		2
#define	LG2_BUF_SIZE	10
#define	BUF_SIZE		(1u << (LG2_BUF_SIZE))

/* in DMA.C */
int dma_from_mem(unsigned char chan, unsigned long adr, unsigned long count,
		bool auto_init);

/* ARGGGGH! Took me a few hours to track this bug down.
The while() loop in write_sample() was getting optimized into nothing.
Evidently, these two lines are NOT the same:
static volatile unsigned char *_dsp_buff_out; */
static unsigned char * volatile _dsp_buff_out;

//static unsigned short _dsp_buff_mem;
static unsigned char *_dsp_buff_in;
static unsigned char *_dsp_buff_base;
static volatile bool _playback_in_progress;

static bool _is_open;
static unsigned short _ioadr;
static unsigned char _mode_byte, _irq, _dma = 3, _mce_bit = 0x40;

typedef struct wss_t wss_t;
struct wss_t
{
	sound_t sound;
	device_t *dev;
};

/*****************************************************************************
*****************************************************************************/
static void ad1848_poke(unsigned char reg, unsigned char val)
{
	unsigned long timeout;

/* wait for initialization, if ongoing, to finish */
	for(timeout = RESYNC_DLY; timeout != 0; timeout--)
	{
		if(in(_ioadr + 0) != 0x80)
			break;
	}
	if(timeout == 0)
	{
		wprintf(L"ad1848_poke: error: timeout\n");
		return;
	}
#ifdef DEBUG
	timeout = RESYNC_DLY - timeout;
	if(timeout != 0)
		wprintf(L"<waited %lu clocks>\n", timeout);
#endif
/* address indirect register reg, and set MCE bit if necessary */
	out(_ioadr + 0, reg | _mce_bit);
/* write indirect register */
	out(_ioadr + 1, val);
}
/*****************************************************************************
*****************************************************************************/
static unsigned char ad1848_peek(unsigned char reg)
{
	unsigned long timeout;
	unsigned char ret_val;

/* wait for initialization, if ongoing, to finish */
	for(timeout = RESYNC_DLY; timeout != 0; timeout--)
	{
		if(in(_ioadr + 0) != 0x80)
			break;
	}
	if(timeout == 0)
	{
		wprintf(L"ad1848_peek: error: timeout\n");
		return -1;
	}
#ifdef DEBUG
	timeout = RESYNC_DLY - timeout;
	if(timeout != 0)
		wprintf(L"<waited %lu clocks>\n", timeout);
#endif
/* address indirect register reg, and set MCE bit if necessary */
	out(_ioadr + 0, reg | _mce_bit);
/* read indirect register */
	ret_val = in(_ioadr + 1);
	return ret_val;
}
/*****************************************************************************
*****************************************************************************/
static int ad1848_leave_mce(void)
{
	unsigned long timeout;

/* wait for initialization, if ongoing, to finish */
	for(timeout = RESYNC_DLY; timeout != 0; timeout--)
	{
		if(in(_ioadr + 0) != 0x80)
			break;
	}
	if(timeout == 0)
	{
		wprintf(L"ad1848_leave_mce: error: timeout\n");
		return -1;
	}
#ifdef DEBUG
	timeout = RESYNC_DLY - timeout;
	if(timeout != 0)
		wprintf(L"<waited %lu clocks>\n", timeout);
#endif
/* clear software MCE bit */
	_mce_bit &= ~0x40;
/* hardware already left MCE? */
	if(((in(_ioadr + 0) ^ _mce_bit) & 0x40) == 0)
		return 0;
/* address indirect register 0, and clear hardware MCE bit */
	out(_ioadr + 0, _mce_bit);
/* wait for ACI to go high, then low. This must be done after
leaving the MCE state, regardless of the state of the ACAL bit. */
	for(timeout = ACI_HIGH_DLY; timeout != 0; timeout--)
	{
		if((ad1848_peek(11) & 0x20) != 0)
			break;
	}
	if(timeout == 0)
	{
		wprintf(L"ad1848_leave_mce: error: ACI signal did "
			"not go high\n");
		return -1;
	}
#ifdef DEBUG
	timeout = ACI_HIGH_DLY - timeout;
	if(timeout != 0)
		wprintf(L"<waited %lu clocks>\n", timeout);
#endif
/* */
	for(timeout = ACI_LOW_DLY; timeout != 0; timeout--)
	{
		if((ad1848_peek(11) & 0x20) == 0)
			break;
	}
	if(timeout == 0)
	{
		wprintf(L"ad1848_leave_mce: error: ACI signal did "
			"not return low\n");
		return -1;
	}
#ifdef DEBUG
	timeout = ACI_LOW_DLY - timeout;
	if(timeout != 0)
		wprintf(L"<waited %lu clocks>\n", timeout);
#endif
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static int ad1848_enter_mce(void)
{
	unsigned long timeout;

/* wait for initialization, if ongoing, to finish */
	for(timeout = RESYNC_DLY; timeout != 0; timeout--)
	{
		if(in(_ioadr + 0) != 0x80)
			break;
	}
	if(timeout == 0)
	{
		wprintf(L"ad1848_enter_mce: error: timeout\n");
		return - 1;
	}
#ifdef DEBUG
	timeout = RESYNC_DLY - timeout;
	if(timeout != 0)
		wprintf(L"<waited %lu clocks>\n", timeout);
#endif
/* set software MCE bit */
	_mce_bit |= 0x40;
/* hardware already in MCE? */
	if(((in(_ioadr + 0) ^ _mce_bit) & 0x40) == 0)
		return 0;
/* address indirect register 0, and set hardware MCE bit */
	out(_ioadr + 0, _mce_bit);
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static int ad1848_detect(unsigned short ioadr)
{
	unsigned char temp, reg_val;
	unsigned long timeout;

/* poll index register of AD1848 until it returns something other than 0x80 */
	for(timeout = POWER_UP_DLY; timeout != 0; timeout--)
	{
		if(in(ioadr) != 0x80)
			break;
	}
	if(timeout == 0)
		return -1;
#ifdef DEBUG
	timeout = POWER_UP_DLY - timeout;
	if(timeout != 0)
		wprintf(L"<waited %lu clocks>\n", timeout);
#endif
/* try writing various AD1848 registers (left and right input gain
and control regs) to see if chip is there */
	for(temp = 0; temp < 2; temp++)
	{
		out(ioadr + 0, temp);
		out(ioadr + 1, 0xAA);
		reg_val = in(ioadr + 1);
		out(ioadr + 1, 0x45);/* b4 is reserved; always write 0 */
		if(reg_val != 0xAA || in(ioadr + 1) != 0x45)
			return -1;
	}
/* try changing the chip revision ID bits (they are read-only) */
	out(ioadr + 0, 0x0C);
	reg_val = in(ioadr + 1);
	out(ioadr + 1, reg_val ^ 0x0F);
	if(((in(ioadr + 1) ^ reg_val) & 0x0F) != 0)
		return -1;
/* found it! */
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static void ad1848_set_volume(sound_t *sound, unsigned char level)
{
	unsigned char temp;

	level >>= 2;
	TRACE1("ad1848_set_volume: %u/255\n", level << 2);
/* convert from gain to attenuation */
	level = 0x3F - level;
/* set left output level */
	temp = ad1848_peek(6) & ~0x3F;
	ad1848_poke(6, temp | level);
/* set right output level */
	temp = ad1848_peek(7) & ~0x3F;
	ad1848_poke(7, temp | level);
}
/*****************************************************************************
*****************************************************************************/
#define	ABS(X)		(((X)<0) ? (-(X)) : (X))

static long ad1848_find_closest_rate(sound_t *sound, long rate)
{
	static const long rates[] =
	{
		5512, 6615, 8000, 9600, 11025, 16000, 18900,
		22050, 27430, 32000, 33075L, 37800L, 44100L, 48000L
	};
	unsigned char temp, closest = 0;
	unsigned long delta;

	delta = -1uL;
	for(temp = 0; temp < sizeof(rates) / sizeof(rates[0]); temp++)
	{
		if((unsigned long)ABS(rate - rates[temp]) < delta)
		{
			delta = ABS(rate - rates[temp]);
			closest = temp;
		}
	}
	TRACE2("ad1848_find_closest_rate: %ld -> %ld\n", rate,
		rates[closest]);
	return rates[closest];
}
/*****************************************************************************
*****************************************************************************/
static int ad1848_set_fmt(sound_t *sound, unsigned char depth, 
						  unsigned char channels, long rate)
{
	_mode_byte = ad1848_peek(8) & ~0x5F;
	switch(depth)
	{
		case 8:		/* _mode_byte |= 0; */		break;
		case 16:	_mode_byte |= 0x40;		break;
		default:
			wprintf(L"ad1848_set_fmt: error: bits/sample (%u) "
				"is not 8 nor 16\n", depth);
			return -1;
	}
	switch(channels)
	{
		case 1:		/* _mode_byte |= 0; */		break;
		case 2:		_mode_byte |= 0x10;		break;
		default:
			wprintf(L"ad1848_set_fmt: error: channels (%u) "
				"is not 1 nor 2\n", channels);
			return -1;
	}
	switch(rate)
	{
		case 8000:	/* _mode_byte |= 0; */		break;
		case 5512:	_mode_byte |= 1;		break;
		case 16000:	_mode_byte |= 2;		break;
		case 11025:	_mode_byte |= 3;		break;
		case 27430:	_mode_byte |= 4;		break;
		case 18900:	_mode_byte |= 5;		break;
		case 32000:	_mode_byte |= 6;		break;
		case 22050:	_mode_byte |= 7;		break;

		case 37800u:	_mode_byte |= 9;		break;

		case 44100u:	_mode_byte |= 11;		break;
		case 48000u:	_mode_byte |= 12;		break;
		case 33075u:	_mode_byte |= 13;		break;
		case 9600:	_mode_byte |= 14;		break;
		case 6615:	_mode_byte |= 15;		break;
		default:
			wprintf(L"ad1848_set_fmt: error: unsupported "
				"sample rate %ld\n", rate);
			return -1;
	}
	TRACE1("ad1848_set_fmt: mode byte=0x%X\n", _mode_byte);
/* must enter MCE state to change audio format or sample rate */
	ad1848_enter_mce();
	ad1848_poke(8, _mode_byte);
	if(ad1848_leave_mce() != 0)
	{
		wprintf(L"ad1848_set_fmt: error leaving MCE state\n");
		return -1;
	}
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static void ad1848_start_playback(sound_t *sound, unsigned long count)
{
	count--;
/* set AD1848 sample count for generation.
Register 15 (LSB of sample count) must be written first */
	ad1848_poke(15, count);
	ad1848_poke(14, count >> 8);
/* select register 9;
    single-channel DMA remains enabled (SDC; b2)
    enable DMA playback (PEN; b0) */
//	ad1848_poke(9, 0x05);
/* select register 9; enable DMA playback (PEN; b0)
No need for MCE if only b0/b1 of register 9 are changed */
	ad1848_poke(9, ad1848_peek(9) | 0x01);
/* unmute outputs, if not already unmuted */
	ad1848_poke(6, ad1848_peek(6) & ~0x80);
	ad1848_poke(7, ad1848_peek(7) & ~0x80);
	_playback_in_progress = true;
}
/*****************************************************************************
*****************************************************************************/
static void ad1848_stop_playback(sound_t *sound, bool wait)
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
		ad1848_poke(6, ad1848_peek(6) | 0x80);
		ad1848_poke(7, ad1848_peek(7) | 0x80);
/* select register 9:
    single-channel DMA remains enabled (SDC; b2)
    DMA playback off (PEN; b0) */
//		ad1848_poke(9, 0x04);
/* select register 9; disable DMA playback (PEN; b0)
No need for MCE if only b0/b1 of register 9 are changed */
		ad1848_poke(9, ad1848_peek(9) & ~0x01);
		_playback_in_progress = false;
	}
}
/*****************************************************************************
*****************************************************************************/
static void ad1848_irq(sound_t *sound, unsigned irq)
{
	unsigned short in_buf, out_buf;

/* clear at AD1848 */
	out(_ioadr + 2, 0);
/* clear at 8259 chips */
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
/* do NOT use printf() in an handler! */
		ad1848_stop_playback(sound, false);
	}
}
/*****************************************************************************
*****************************************************************************/
static int write_sample(sound_t *sound, short left, short right)
{
	unsigned short in_buf, out_buf, new_in_buf, count;
	unsigned char depth, channels;
	bool wait = false;

	depth = (_mode_byte & 0x40) ? 16 : 8;
	channels = (_mode_byte & 0x10) ? 2 : 1;
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
	if(depth == 16)
	{
		write_le16(_dsp_buff_in, left);
		_dsp_buff_in += 2;
		if(channels == 2)
		{
			write_le16(_dsp_buff_in, right);
			_dsp_buff_in += 2;
			count = BUF_SIZE >> 2;
		}
		else
			count = BUF_SIZE >> 1;
	}
	else /* if(depth == 8) */
	{
		left >>= 8;
		*_dsp_buff_in = left;
		_dsp_buff_in++;
		if(channels == 2)
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
		ad1848_start_playback(sound, count);
	}
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static bool ad1848_open(sound_t *sound)
{
	static const unsigned char init_vals[] =
	{
		0xA8, 0xA8,	/* input from line, input gain=0 */
		0x08, 0x08,	/* unmute aux #1, midrange aux #1 attenuate */
		0x08, 0x08,	/* unmute aux #2, midrange aux #2 attenuate */
		0x90, 0x90,	/* MUTE output, midrange output attenuate */
		0x0C,		/* 48000 Hz, mono, 8-bit, PCM */
/* set PPIO (b6; for PIO playback), set ACAL (b3; auto-calibrate);
set SDC (b2; single-channel DMA) */
		0x4D,
		0x02,		/* enable interrupts */
		0x00, 0x00, 0x00,
		0x00, 0x00	/* every sample */
	};
	static const unsigned short base_io_adr[] =
	{
		0x530, 0x604, 0xE80, 0xF40
	};
	unsigned short temp;
	unsigned long temp2;
//	unsigned char dma_mask;
	int err = -1;
	dword index;
	wss_t *wss = (wss_t*) sound;

/* already open? */
	if(_is_open)
		return 0;
/* PROBE FOR HARDWARE */
	TRACE0("ad1848_open: probing for Windows Sound System\n");
/* probe for AD1848 chip 4 bytes above base I/O address */
	temp = 0;
	for(; temp < sizeof(base_io_adr) / sizeof(base_io_adr[0]); temp++)
	{
		_ioadr = base_io_adr[temp] + 4;
		TRACE1("\t""probing at I/O=0x%03X...\n", _ioadr);
		err = ad1848_detect(_ioadr);
		if(err == 0)
			break;
	}
	if(err != 0)
	{
		wprintf(L"\t""error: Windows Sound System not detected\n");
		return false;
	}
/* enter Mode Change Enable (MCE) mode to initialize registers,
esp. to set the CPIO bit in register 9 (enables PIO mode playback) */
	TRACE0("entering MCE to enable PIO mode playback\n");
	if(ad1848_enter_mce() != 0)
	{
		wprintf(L"\t""error entering MCE state\n");
		return false;
	}
	for(temp = 0; temp < 16; temp++)
		ad1848_poke(temp, init_vals[temp]);
/* leave MCE (else IRQ auto-probe will not work) */
	TRACE0("leaving MCE mode\n");
	if(ad1848_leave_mce() != 0)
	{
		wprintf(L"\t""error leaving MCE state\n");
		return false;
	}

#if 0
/* PROBE FOR IRQ */
	TRACE0("probing AD1848 chip IRQ...\n");
//	irq_mask = 0x0E80;	/* IRQs 11, 10, 9, 7 */
	irq_mask = 0x0EA0;	/* IRQs 11, 10, 9, 7, 5 */
/* are one or more of those IRQs busy? */
	if(irq_start_probe(&irq_mask) != 0)
	{
		if(irq_mask == 0)	/* they're ALL busy */
		{
			wprintf(L"\t""error: all IRQs in use\n");
			return false;
		}
/* try again, avoiding busy IRQs returned by first call */
		if(irq_start_probe(&irq_mask) != 0)
		{
			wprintf(L"\t""error: could not start IRQ probe\n");
			return false;
		}
	}
/* xxx - I don't know why I have to set this a second time..
OSS driver for AD1848 says
	"Write to I8 starts resynchronization. Wait until it completes."
so maybe I have to do that _immediately_ after writing register 8 above,
not waiting until regs 9-15 have also been written. */
	ad1848_poke(15, 0);
	ad1848_poke(14, 0);
/* use PIO mode playback to trigger IRQs */
	TRACE0("\t""starting PIO mode playback...\n");
/* MIN_IRQS (3) loops, 1 millisecond/loop == 3 millisecond probe time */
	for(temp = MIN_IRQS; temp != 0; temp--)
	{
/* poll to see if chip ready for a sample */
		if((in(_ioadr + 2) & 2) != 0)
/* write zero sample */
		{
			out(_ioadr + 3, 0);
/* clear the generated IRQ */
			out(_ioadr + 2, 0);
		}
/* with sampling rate of 48000 Hz, this could go as low as
21 microseconds (usleep(21)?) for faster probing */
		msleep(1);
	}
/* end probe */
	err = irq_end_probe(&irq_mask);
/* IRQ probe results: */
	TRACE2("\t""irq_end_probe() returned %d; "
		"irq_mask=0x%04X\n", temp, irq_mask);
	if(err < 0 || irq_mask == 0)
	{
		wprintf(L"\t""error: could not probe IRQ\n"
			"Try using SOFTSET to set board IRQ and DMA\n");
		return -1;
	}
	_irq = err;
/* enter MCE again to turn off PIO mode */
	TRACE0("entering MCE to enable DMA mode playback\n");
	if(ad1848_enter_mce() != 0)
	{
		wprintf(L"\t""error entering MCE state\n");
		return -1;
	}
#endif

	index = devFindResource(wss->dev->config, dresIrq, 0);
	if (index == (dword) -1)
		return false;

	_irq = wss->dev->config->resources[index].u.irq;

/* xxx - don't know why I have to leave auto-calibrate turned on...
	ad1848_poke(9, 0x04);	SDC */
	ad1848_poke(9, 0x0C); /* ACAL, SDC */
	TRACE0("leaving MCE mode\n");
	if(ad1848_leave_mce() != 0)
	{
		wprintf(L"\t""error leaving MCE state\n");
		return -1;
	}



/* clear any dangling IRQ at AD1848 chip */
	out(_ioadr + 2, 0);
/* enable interrupts at 8259 controller chips */
	temp = 1;
	temp <<= _irq;
	out(0x21, in(0x21) & ~temp);
	temp >>= 8;
	out(0xA1, in(0xA1) & ~temp);

	_dsp_buff_base = 
		(unsigned char*) memAllocLowSpan((BUF_SIZE * NUM_BUFS) / PAGE_SIZE);
	if (_dsp_buff_base == NULL)
	{
		wprintf(L"\tFailed to allocate %u pages\n", (BUF_SIZE * NUM_BUFS) / PAGE_SIZE);
		return false;
	}

	_dsp_buff_in = _dsp_buff_base;
	_dsp_buff_out = _dsp_buff_base;
	temp2 >>= 4;
/* */
	wprintf(L"\tAD1848 chip at I/O=0x%03X, IRQ=%u, DMA=%u\n",
		_ioadr, _irq, _dma);
	wprintf(L"\t%u DSP buffers at %04lX:0000 (%u bytes each, "
		"%u bytes total)\n", NUM_BUFS, temp2, BUF_SIZE,
		NUM_BUFS * BUF_SIZE);
/* arm auto-initializing DMA */
	err = dma_from_mem(_dma, (addr_t) _dsp_buff_base,
		NUM_BUFS * BUF_SIZE, true);
	if(err != 0)
		return err;
/* crap, don't forget this, or ad1848_close() won't work */
	_is_open = true;
/* take over vector */
	devRegisterIrq(wss->dev, _irq, true);
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static void ad1848_close(sound_t *sound)
{
	addr_t addr;

	TRACE0("ad1848_close\n");
/* already closed? */
	if(!_is_open)
		return;
/* abort playback */
	if(_playback_in_progress)
	{
		TRACE0("aborting playback...\n");
		ad1848_stop_playback(sound, false);
	}

	for (addr = 0; addr < BUF_SIZE * NUM_BUFS; addr += PAGE_SIZE)
		memFree((addr_t) _dsp_buff_base + addr);

/* restore old vector */
	devRegisterIrq(((wss_t*) sound)->dev, _irq, true);
	_is_open = false;
}
/*****************************************************************************
*****************************************************************************/
wss_t wss_drv =
{
	{
		ad1848_open,
		ad1848_find_closest_rate,
		ad1848_set_fmt,
		ad1848_set_volume,
		write_sample,
		ad1848_stop_playback,
		ad1848_close,
		ad1848_irq,
	},
	NULL
};

sound_t *wss_init(device_t *dev)
{
	wss_drv.dev = dev;
	return &wss_drv.sound;
}