/*
 *
 *	Driver for ESS AudioDrive SOLO-1 (ES1938/41/46)
 *	PCI vendor/device = 125D/1969
 *	10/08/01	By Tim Robinson for The Möbius
 *
 */

#include "sound.h"
#include <kernel/sys.h>

/* DSP buffers -- large for few interrupts, small for responsiveness */
#define	NUM_BUFS	2
#define	LG2_BUF_SIZE	10
#define	BUF_SIZE	(1u << (LG2_BUF_SIZE))

typedef struct solo_t solo_t;
struct solo_t
{
	sound_t sound;
	device_t *device;
	word iobase, sbbase, vcbase;
};

#define PCI_VENDOR_ESS			0x125d
#define PCI_DEVICE_ESS_SOLO1	0x1969

static bool _is_open;
static volatile bool _playback_in_progress;
/* THIS IS WRONG: static volatile unsigned char far *_dsp_buff_out; */
static unsigned char * volatile _dsp_buff_out;
static unsigned char *_dsp_buff_mem, *_dsp_buff_base, *_dsp_buff_in;

//! Send a SOLO-1 command or write a data byte
void solo_command(word sbbase, byte command)
{
	TRACE2("solo_command(%x, %x", sbbase, command);
	while (in(sbbase + 0xC) & 0x80)
		;

	TRACE0(")\n");
	out(sbbase + 0xC, command);
}

void solo_write(word sbbase, byte reg, byte data)
{
	solo_command(sbbase, reg);
	solo_command(sbbase, data);
}

//! Read a SOLO-1 register
byte solo_read(word sbbase, byte reg)
{
	byte val;

	solo_command(sbbase, 0xC0);
	solo_command(sbbase, reg);
	
	TRACE2("solo_read(%x, %x", sbbase, reg);
	while (((val = in(sbbase + 0xE)) & 0x80) == 0)
		TRACE1("%02x\r", val);

	val = in(sbbase + 0xA);
	TRACE1(") = %x\n", val);
	return val;
}

/*****************************************************************************
*****************************************************************************/
void solo_set_volume(sound_t *sound, unsigned char level)
{
	TRACE1("solo_set_volume: %u/255\n", level);
}
/*****************************************************************************
*****************************************************************************/
long solo_find_closest_rate(sound_t *sound, long rate)
{
	TRACE2("solo_find_closest_rate: %ld -> %ld\n", rate, rate);
	return rate;
}

/*****************************************************************************
*****************************************************************************/
int solo_set_fmt(sound_t *sound, unsigned char depth, 
						unsigned char channels, long rate)
{
	byte mode;
	solo_t *solo = (solo_t*) sound;

	/* Set to normal DMA mode */
	solo_write(solo->sbbase, 0xB8, 0);
	
	/* Read mono/stereo */
	mode = solo_read(solo->sbbase, 0xA8);

	switch(channels)
	{
		case 1:
			mode = (mode & ~3) | 2;
			break;
		case 2:
			mode = (mode & ~3) | 1;
			break;
		default:
			wprintf(L"solo_set_fmt: error: channels (%u) "
				"is not 1 nor 2\n", channels);
			return -1;
	}
	
	/* Write mono/stereo */
	solo_write(solo->sbbase, 0xA8, mode);

	/* Read transfer mode */
	mode = solo_read(solo->sbbase, 0xB9);

	switch(depth)
	{
		case 8:
			/* Set single transfer DMA: 1 byte per request */
			mode = (mode & ~3) | 0;
			break;
		case 16:
			/* Set demand transfer DMA: 2 bytes per request */
			mode = (mode & ~3) | 1;
			break;

			/* xxx - bits 1:0 == 1 => 32 bits/sample? */

		default:
			wprintf(L"solo_set_fmt: error: bits/sample (%u) "
				"is not 8 nor 16\n", depth);
			return -1;
	}

	/* Write transfer mode */
	solo_write(solo->sbbase, 0xB9, mode);

	switch(rate)
	{
		case 8000:
		case 11025:
		case 22050:
		case 44100u:
			break;
		default:
			wprintf(L"solo_set_fmt: error: unsupported "
				"sample rate %ld\n", rate);
			return -1;
	}
	return 0;
}
/*****************************************************************************
*****************************************************************************/
void solo_start_playback(sound_t *sound, unsigned long count)
{
	_playback_in_progress = true;
}
/*****************************************************************************
*****************************************************************************/
void solo_stop_playback(sound_t *sound, bool wait)
{
	if(wait)
	{
		TRACE0("waiting...\n");
		while(_playback_in_progress)
			/* nothing */;
	}
	else
	{
		_playback_in_progress = false;
	}
}
/*****************************************************************************
*****************************************************************************/
int solo_write_sample(sound_t *sound, short left, short right)
{
	unsigned short in_buf, out_buf, new_in_buf, count;
	unsigned char depth, channels;
	bool wait = false;

	if (_dsp_buff_out == NULL ||
		_dsp_buff_in == NULL)
		return -1;

	depth = 8;
	channels = 1;
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
/* assumes LITTLE ENDIAN 16-bit samples for this sound hardware */
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
		solo_start_playback(sound, count);
	}
	return 0;
}

#define solo_read32(cfg, reg) \
	pciRead(cfg->pci_bus, cfg->pci_dev, cfg->pci_func, reg, 4)
#define solo_write32(cfg, reg, val) \
	pciWrite(cfg->pci_bus, cfg->pci_dev, cfg->pci_func, reg, val, 4)

/*****************************************************************************
detect sound hardware, allocate DSP buffers, install interrupt hanadler
*****************************************************************************/
bool solo_open(sound_t *sound)
{
	solo_t *solo = (solo_t*) sound;
	dword index, end;
	volatile word temp;
	byte val;
	device_config_t *cfg;

	cfg = solo->device->config;
	/* The SOLO-1 is a PCI device: check configuration */
	if (cfg == NULL ||
		cfg->vendor_id != PCI_VENDOR_ESS ||
		cfg->device_id != PCI_DEVICE_ESS_SOLO1)
	{
		TRACE0("solo_open: invalid PCI config\n");
		return false;
	}

	/* index should = zero */
	index = devFindResource(cfg, dresIo, 0);
	if (index == (dword) -1)
	{
		TRACE0("solo_open: no IO port configured\n");
		return false;
	}

	solo->iobase = cfg->resources[index].u.io.base;
	solo->sbbase = cfg->resources[index + 1].u.io.base;
	solo->vcbase = cfg->resources[index + 2].u.io.base;
	TRACE3("solo_open: resetting device at io=%x sb=%x vc=%x\n", 
		solo->iobase, solo->sbbase, solo->vcbase);
	
	/*
	 * Configure SOLO-1 in native mode
	 */

	/*
	 * Set up DDMA control register: copy (VCBase register) | 1 to DDMA 
	 *	control register
	 */
	solo_write32(cfg, 0x60, solo_read32(cfg, 0x18) | 1);
	
	/* Set DMA/IRQ policy */
	index = solo_read32(cfg, 0x50);
	index &= ~0x6700;	/* clear bits 14:13, 10:8 */
	solo_write32(cfg, 0x50, index);

	/* Enable extended audio 1 DAC operation */
	out(solo->sbbase + 6, 3);
	
	/*
	 * Reset the SOLO-1
	 */

	/* Pulse sbbase + 6, bit 0 */
	//out(sbbase + 6, in(sbbase + 6) | 1);
	temp = in(solo->sbbase + 6);
	out(solo->sbbase + 6, in(solo->sbbase + 6) & ~1);

	/* Wait at least 1ms for sbbase + 0xE bit 7 to be set */
	end = sysUpTime() + 10;
	while ((in(solo->sbbase + 0xE) & 0x80) == 0 &&
		(val = in(solo->sbbase + 0xA)) != 0xAA)
	{
		if (sysUpTime() >= end)
		{
			TRACE1("solo_open: timeout (%02x)\n", val);
			return false;
		}
	}

	solo_command(solo->sbbase, 0xc6);

	/* Rate etc. will be set in solo_set_fmt */

	_is_open = true;
	return true;
}
/*****************************************************************************
*****************************************************************************/
void solo_close(sound_t *sound)
{
	TRACE0("solo_close\n");
/* already closed? */
	if(!_is_open)
	{
		wprintf(L"solo_close: sound hardware already closed\n");
		return;
	}
/* abort playback */
	if(_playback_in_progress)
	{
		TRACE0("aborting playback...\n");
		solo_stop_playback(sound, false);
	}
/* free DSP buffers; restore old interrupt vector */
	_is_open = false;
}
/*****************************************************************************
*****************************************************************************/
solo_t solo_drv =
{
	{
		solo_open,
		solo_find_closest_rate,
		solo_set_fmt,
		solo_set_volume,
		solo_write_sample,
		solo_stop_playback,
		solo_close,
	},
	NULL,
	0, 0, 0
};

sound_t *solo_init(device_t *dev)
{
	solo_drv.device = dev;
	return &solo_drv.sound;
}