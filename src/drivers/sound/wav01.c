/*****************************************************************************
linear PCM
sub-format 1 of .WAV format
*****************************************************************************/
#include <stdio.h> /* fread() */
#include "sound.h"

/* in WAV.C */
int wav_validate(sound_info_t *info, unsigned sub_type);
/*****************************************************************************
*****************************************************************************/
static int get_sample(sound_info_t *info, short *left, short *right)
{
	unsigned char buffer[4];

	if(info->depth == 16 && info->channels == 2)
	{
		if(fread(buffer, 1, 4, info->infile) != 4)
			return -1;
		*left = read_le16(buffer + 0);
		*right = read_le16(buffer + 2);
	}
	else if(info->depth == 16 && info->channels == 1)
	{
		if(fread(buffer, 1, 2, info->infile) != 2)
			return -1;
		*left = read_le16(buffer + 0);
		*right = *left;
	}
	else if(info->depth == 8 && info->channels == 2)
	{
		if(fread(buffer, 1, 2, info->infile) != 2)
			return -1;
		*left = buffer[0] << 8;
		*right = buffer[1] << 8;
	}
	else if(info->depth == 8 && info->channels == 1)
	{
		if(fread(buffer, 1, 1, info->infile) != 1)
			return -1;
		*left = buffer[0] << 8;
		*right = *left;
	}
	else
		return -1;
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static int validate(sound_info_t *info)
{
	fseek(info->infile, 0, SEEK_SET);
/* WAV sub-format #1 is linear PCM */
	return wav_validate(info, 1);
}
/*****************************************************************************
*****************************************************************************/
codec_t _wav01 =
{
	".wav linear PCM", validate, get_sample
};
