/*****************************************************************************
DSP Group True Speech
sub-format 34 of .WAV format

I haven't checked, but I think the True Speech codec
is either commerical ($) or non-disclosure (NDA).
*****************************************************************************/
#include "sound.h"

/* in WAV.C */
int wav_validate(sound_info_t *info, unsigned sub_type);
/*****************************************************************************
*****************************************************************************/
static int validate(sound_info_t *info)
{
/* WAV sub-format #34 is True Speech */
	if(wav_validate(info, 34) == 0)
		printf("SORRY, TRUE SPEECH NOT SUPPORTED...");
	return -1;
}
/*****************************************************************************
*****************************************************************************/
static int get_sample(sound_info_t *info, short *left, short *right)
{
	return -1;
}
/*****************************************************************************
*****************************************************************************/
codec_t _wav34 =
{
	".wav TrueSpeech", validate, get_sample
};
