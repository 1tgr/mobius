/*****************************************************************************
mu-law
sub-format 1 of .au format
*****************************************************************************/
#include <stdio.h> /* EOF, fgetc() */
#include "sound.h"

/* in MISC.C */
short mulaw_expand(unsigned char data);

/* in AU.C */
int au_validate(sound_info_t *info, unsigned sub_type);
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
	fseek(info->infile, 0, SEEK_SET);
	info->depth = 16;
/* au sub-format #1 is u-law */
	return au_validate(info, 1);
}
/*****************************************************************************
*****************************************************************************/
codec_t _au01 =
{
	".au mu-law", validate, get_sample
};
