/*****************************************************************************
sndpkg release 2 - sound file player for DOS
Chris Giese <geezer@execpc.com>, http://www.execpc.com/~geezer
Release date: Jan 3, 2001
*****************************************************************************/
#include <string.h> /* strlen(), strcmpi() */
#include <stdio.h> /* FILE, printf(), fopen(), fread(), fclose() */
#include <conio.h> /* kbhit(), getch() */
#include <dos.h> /* MK_FP() */
#include "sound.h"

/* The nice thing about standards is... */
#if defined(__DJGPP__)
#define	strcmpi(A, B)	stricmp(A, B)
#endif

extern sound_t wss_drv, sb_drv;

/* the Sound Blaster driver (sb_drv) does not yet work */
static sound_t *_drv = &wss_drv;
//static driver_t *_drv = &sb_drv;

extern codec_t _wav01, _wav02, _wav06, _wav07, _wav34, _wav49, _wav85;
extern codec_t _au01, _au023, _au27;

static codec_t *_codecs[] =
{
/* 0 */	&_wav01, &_wav02, &_wav06, &_wav07, &_wav34, &_wav49, &_wav85,
/* 7 */	&_au01, &_au023, &_au27
/* 10 */
};
/*****************************************************************************
*****************************************************************************/
static int play(char *filename)
{
	static short volume = 224;
	static bool is_open;
	unsigned short start_codec, curr_codec, key;
	short left, right;
	sound_info_t info;
	codec_t *codec;
	bool wait;
	char *ext;

	wait = true;
	info.fsd = NULL;
/* open .WAV file */
	info.infile = fopen(filename, "rb");
	if(info.infile == NULL)
	{
		wprintf(L"Can't open file '%s'\n", filename);
		return 1;
	}
/* deduce file type from filename extension.
Not to fear, if we guess wrong, all codecs will be checked.
Start checking with codec #0 (.WAV files) */
	start_codec = 0;
	ext = filename + strlen(filename);
	do
	{
		ext--;
		if(*ext == '.')
		{
/* if filename ends with ".au", start checking with codec #7 */
			if(!strcmpi(ext, ".au"))
				start_codec = 7;
/*			else if(!strcmpi(ext, ".voc"))
				start_codec = ?;
			else if(!strcmpi(ext, ".mp3"))
				start_codec = ?; */
			break;
		}
	} while(ext > filename);
/* check format */
	curr_codec= start_codec;
	do
	{
		codec = _codecs[curr_codec];
		wprintf(L"\tchecking %s...", codec->name);
		if(codec->validate(&info) == 0)
		{
			wprintf(L"OK\n");
			goto OK;
		}
		wprintf(L"no\n");
		curr_codec++;
		if(curr_codec >= sizeof(_codecs) / sizeof(_codecs[0]))
			curr_codec = 0;
	} while(curr_codec != start_codec);
	wprintf(L"Unknown or unsupported file format or codec\n");
	return 2;
OK:
	wprintf(L"File '%s': %u bits/sample, %ld samples/sec, %s\n",
		filename, info.depth, info.rate,
		info.channels == 1 ? "mono" : "stereo");
	if(!is_open)
	{
/* detect hardware, activate it, alloc buffers, install interrupt handler */
		if(_drv->open() != 0)
		{
			wprintf(L"Error opening sound hardware\n");
			fclose(info.infile);
			return 1;
		}
/* other things to be done only once */
		wprintf(L"PgUp, PgDn set volume; Esc stops; Pause pauses\n");
		_drv->set_volume(volume);
		is_open = true;
	}
	info.rate = _drv->find_closest_rate(info.rate);
	if(_drv->set_fmt(info.depth, info.channels, info.rate) != 0)
	{
		wprintf(L"Error setting sound format (depth %u, channels %u, "
			"rate %lu)\n", info.depth, info.channels, info.rate);
		fclose(info.infile);
		return 1;
	}
/* play it */
	while(codec->get_sample(&info, &left, &right) == 0)
	{
		if(kbhit())
		{
			key = getch();
			if(key == 0)
				key = 0x100 | getch();
			if(key == 27)
			{
				wait = false; /* abort playback */
				break;
			}
			else if(key == 0x149)
			{
				volume += 4;
				if(volume > 255)
					volume = 255;
				_drv->set_volume(volume);
			}
			else if(key == 0x151)
			{
				volume -= 4;
				if(volume < 0)
					volume = 0;
				_drv->set_volume(volume);
			}
		}
		if(_drv->write_sample(left, right) != 0)
		{
			wait = false;
			break;
		}
	}
/* wait until playback is done (or abort playback) */
	_drv->stop_playback(wait);
	fclose(info.infile);
	return 0;
}
/*****************************************************************************
*****************************************************************************/
int main(int arg_c, char *arg_v[])
{
	int i;

	if(arg_c < 2)
	{
		wprintf(L"Plays .WAV and .au files\n");
		return 1;
	}
	for(i = 1; i < arg_c; i++)
		(void)play(arg_v[i]);
	_drv->close();
	return 0;
}
