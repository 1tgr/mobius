/*****************************************************************************
linear PCM
(8-bit) sub-format 2 of .au format
(16-bit) sub-format 3 of .au format

Sox appears to make 16-bit linear PCM .au files when
asked to convert 16-bit .wav files to .au format
*****************************************************************************/
#include <stdio.h> /* EOF, fgetc() */
#include "sound.h"

/* in AU.C */
int au_validate(sound_info_t *info, unsigned sub_type);
/*****************************************************************************
*****************************************************************************/
static int get_sample(sound_info_t *info, short *left, short *right)
{
	unsigned char buffer[4];

	if(info->depth == 16 && info->channels == 2)
	{
		if(fread(buffer, 1, 4, info->infile) != 4)
			return -1;
		*left = read_be16(buffer + 0);
		*right = read_be16(buffer + 2);
	}
	else if(info->depth == 16 && info->channels == 1)
	{
		if(fread(buffer, 1, 2, info->infile) != 2)
			return -1;
		*left = read_be16(buffer + 0);
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
	int err;

	fseek(info->infile, 0, SEEK_SET);
/* au sub-format #2 is 8-bit PCM */
	info->depth = 8;
	err = au_validate(info, 2);
	if(err != 0)
	{
/* au sub-format #3 is 16-bit PCM */
		info->depth = 16;
		err = au_validate(info, 3);
	}
	return err;
}
/*****************************************************************************
*****************************************************************************/
codec_t _au023 =
{
	".au linear PCM", validate, get_sample
};
