/*****************************************************************************
"skeleton" driver
*****************************************************************************/
#include "sound.h" /* DEBUG(X), among others */

/* DSP buffers -- large for few interrupts, small for responsiveness */
#define	NUM_BUFS	2
#define	LG2_BUF_SIZE	10
#define	BUF_SIZE	(1u << (LG2_BUF_SIZE))

static bool _is_open;
static volatile bool _playback_in_progress;
/* THIS IS WRONG: static volatile unsigned char far *_dsp_buff_out; */
static unsigned char far * volatile _dsp_buff_out;
static unsigned char far *_dsp_buff_mem, *_dsp_buff_base, *_dsp_buff_in;
/*****************************************************************************
*****************************************************************************/
static void skel_set_volume(unsigned char level)
{
	DEBUG(printf("skel_set_volume: %u/255\n", level);)
}
/*****************************************************************************
*****************************************************************************/
static long skel_find_closest_rate(long rate)
{
	DEBUG(printf("skel_find_closest_rate: %ld -> %ld\n", rate,
		rate);)
	return rate;
}
/*****************************************************************************
*****************************************************************************/
static int skel_set_fmt(unsigned char depth, unsigned char channels,
		long rate)
{
	switch(depth)
	{
		case 8:
			break;
		case 16:
			break;
		default:
			printf("skel_set_fmt: error: bits/sample (%u) "
				"is not 8 nor 16\n", depth);
			return -1;
	}
	switch(channels)
	{
		case 1:
			break;
		case 2:
			break;
		default:
			printf("skel_set_fmt: error: channels (%u) "
				"is not 1 nor 2\n", channels);
			return -1;
	}
	switch(rate)
	{
		case 8000:
		case 11025:
		case 22050:
		case 44100u:
			break;
		default:
			printf("skel_set_fmt: error: unsupported "
				"sample rate %ld\n", rate);
			return -1;
	}
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static void skel_start_playback(unsigned long count)
{
	_playback_in_progress = true;
}
/*****************************************************************************
*****************************************************************************/
static void skel_stop_playback(bool wait)
{
	if(wait)
	{
		DEBUG(printf("waiting...\n");)
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
static int write_sample(short left, short right)
{
	unsigned short in_buf, out_buf, new_in_buf, count;
	unsigned char depth, channels;
	bool wait = false;

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
			DEBUG(printf("waiting...");)
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
		printf("*** kicking playback ***\n");
		skel_start_playback(count);
	}
	return 0;
}
/*****************************************************************************
detect sound hardware, allocate DSP buffers, install interrupt hanadler
*****************************************************************************/
static int skel_open(void)
{
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static void skel_close(void)
{
	DEBUG(printf("skel_close\n");)
/* already closed? */
	if(!_is_open)
	{
		printf("skel_close: sound hardware already closed\n");
		return;
	}
/* abort playback */
	if(_playback_in_progress)
	{
		DEBUG(printf("aborting playback...\n");)
		skel_stop_playback(false);
	}
/* free DSP buffers; restore old interrupt vector */
	_is_open = false;
}
/*****************************************************************************
*****************************************************************************/
sound_t skel_drv =
{
	skel_open,
	skel_find_closest_rate,
	skel_set_fmt,
	skel_set_volume,
	write_sample,
	skel_stop_playback,
	skel_close,
};
