/*****************************************************************************
mu-law
sub-format 7 of .WAV format
*****************************************************************************/
#include <stdio.h> /* EOF, fgetc() */
#include "sound.h"

/* in MISC.C */
short mulaw_expand(unsigned char data);

/* in WAV.C */
int wav_validate(sound_info_t *info, unsigned sub_type);
/*****************************************************************************
*****************************************************************************/
static int get_sample(sound_info_t *info, short *left, short *right)
{
	int temp;

	temp = fgetc(info->infile);
	if(temp == EOF)
		return -1;
	info->bytes_left--;
	*left = mulaw_expand(temp);
	if(info->channels == 1)
	{
		*right = *left;
		return 0;
	}
	temp = fgetc(info->infile);
	if(temp == EOF)
		return -1;
	info->bytes_left--;
	*right = mulaw_expand(temp);
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static int validate(sound_info_t *info)
{
	int err;

	fseek(info->infile, 0, SEEK_SET);
/* WAV sub-format #7 is u-law */
	err = wav_validate(info, 7);
	info->depth = 16;
	return err;
}
/*****************************************************************************
*****************************************************************************/
codec_t _wav07 =
{
	".wav mu-law", validate, get_sample
};

