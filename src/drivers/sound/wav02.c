/*****************************************************************************
Microsoft Adaptive Differential PCM (ADPCM)
sub-format 2 of .WAV format

Source:	file MSADPCM.C of Waveform Conversion Sample Application,
	Microsoft Multimedia Systems Group
	Copyright (C) Microsoft Corp. 1991, 1992.
*****************************************************************************/
#include <stdlib.h> /* malloc() */
#include <string.h> /* memcmp() */
#include <stdio.h>  /* FILE, EOF, fread(), fgetc() */
#include "sound.h" /* sound_info_t */

typedef struct
{
	short delta, coeff1, coeff2;
	short samp1, samp2;
	unsigned char predictor;
} chaninfo_t;

typedef struct
{
	unsigned samples_per_frame, samples_left;
	chaninfo_t chan_info[2];
	unsigned char byte;
} adpcm_t;
/*****************************************************************************
*****************************************************************************/
static void do_adpcm(chaninfo_t *chan_info, short *sample, signed char nybble)
{
/* "Fixed point delta adaption table" */
	static const short gai_p4[] =
	{
		230, 230, 230, 230, 307, 409, 512, 614,
		768, 614, 512, 409, 307, 230, 230, 230
	};
	long predict, output, old_delta;

/* update delta */
	old_delta = chan_info->delta;
	chan_info->delta = (gai_p4[nybble] * old_delta) >> 8;
	if(chan_info->delta < 16)
		chan_info->delta = 16;
/* sign-extend nybble */
	if(nybble & 0x08)
		nybble -= 0x10;
/* predict next sample */
	predict = ((long)chan_info->samp1 * chan_info->coeff1
		+ (long)chan_info->samp2 * chan_info->coeff2) >> 8;
/* reconstruct original PCM */
	output = nybble * old_delta + predict;
/* clip to 16 bits */
	if(output > 32767)
		output = 32767;
	else if(output < -32768L)
		output = -32768L;
/* update previouses */
	chan_info->samp2 = chan_info->samp1;
	chan_info->samp1 = output;
/* return the sample */
	*sample = output;
}
/*****************************************************************************
*****************************************************************************/
static int get_sample(sound_info_t *info, short *left, short *right)
{/* technically, the GaiCoeff values should be read from the "fmt "
block of the .WAV file (ADPCM has 32 extra bytes in that block versus
regular PCM) instead of being hard-coded like this. */
	static const short gai_coeff1[] =
	{
		256,  512,  0, 192, 240,  460,  392
	};
	static const short gai_coeff2[] =
	{
		0, -256,  0,  64,   0, -208, -232
	};
	chaninfo_t *left_info, *right_info;
	unsigned char buffer[14];
	adpcm_t *adpcm_info;

	adpcm_info = (adpcm_t *)info->fsd;
	left_info = adpcm_info->chan_info + 0;
	right_info = adpcm_info->chan_info + 1;
	if(adpcm_info->samples_left == 0)
/* need to load a new mono ADPCM frame */
	{
		DEBUG(printf("ftell()=%5lu: ", ftell(info->infile));)
		if(info->channels == 1)
		{
			if(fread(buffer, 1, 7, info->infile) != 7)
				return -1;
			info->bytes_left -= 7;
/* 7-byte frame header: 8-bit predictor... */
			left_info->predictor = buffer[0];
/* ...16-bit delta */
			left_info->delta = read_le16(buffer + 1);
/* ...16-bit Previous sample */
			left_info->samp1 = read_le16(buffer + 3);
/* ...16-bit next-to-Previous sample */
			left_info->samp2 = read_le16(buffer + 5);
		}
/* need to load a new stereo ADPCM frame */
		else //if(info->channels == 2)
		{
			if(fread(buffer, 1, 14, info->infile) != 14)
				return -1;
			info->bytes_left -= 14;
/* 7-byte frame header: 8-bit Predictors... */
			left_info->predictor = buffer[0];
			right_info->predictor = buffer[1];
/* ...16-bit Deltas */
			left_info->delta = read_le16(buffer + 2);
			right_info->delta = read_le16(buffer + 4);
/* ...16-bit Previous samples */
			left_info->samp1 = read_le16(buffer + 6);
			right_info->samp1 = read_le16(buffer + 8);
/* ...16-bit next-to-Previous samples */
			left_info->samp2 = read_le16(buffer + 10);
			right_info->samp2 = read_le16(buffer + 12);
//			DEBUG(printf("right: predictor=%u, Index=%4d, "
//				"Prev=%5d, samp2=%5d\n", right_info->predictor,
//				right_info->delta, right_info->samp1,
}//				right_info->samp2);) }
//		DEBUG(printf("left: predictor=%u, Index=%4d, Prev=%5d, "
//			"samp2=%5d\n", left_info->predictor,
//			left_info->delta, left_info->samp1,
//			left_info->samp2);)
/* check Predictors. As with the GaiCoef values, the limit 6 should also
probably come from the ADPCM "fmt " block instead of being hard-coded. */
		if(left_info->predictor > 6)
		{
			DEBUG(printf("LPredictor > 6\n");)
/* xxx - return -2 for corrupt file? */
			return -2;
		}
		if(right_info->predictor > 6)
		{
			DEBUG(printf("RPredictor > 6\n");)
			return -2;
		}
/* cache coefficients 1 and 2 for each channel */
		left_info->coeff1 = gai_coeff1[left_info->predictor];
		left_info->coeff2 = gai_coeff2[left_info->predictor];
		right_info->coeff1 = gai_coeff1[right_info->predictor];
		right_info->coeff2 = gai_coeff2[right_info->predictor];
		adpcm_info->samples_left = adpcm_info->samples_per_frame;
/* return next-to-previous sample, from frame header */
		*left = left_info->samp2;
		if(info->channels == 1)
			*right = *left;
		else
			*right = right_info->samp2;
		adpcm_info->samples_left--;
		return 0;
	}
/* return previous sample, from frame header */
	if(adpcm_info->samples_left == adpcm_info->samples_per_frame - 1)
	{
		*left = left_info->samp1;
		if(info->channels == 1)
			*right = *left;
		else
			*right = right_info->samp1;
		adpcm_info->samples_left--;
		return 0;
	}
/* here's where it gets hairy */
	{
		int temp;
		signed char nybble;

		if(info->channels == 1)
		{
/* odd sample: get least-significant nybble of previously-fetched byte
xxx - this works only if samples_per_frame is always even */
			if((adpcm_info->samples_left & 1) != 0)
				nybble = adpcm_info->byte & 0x0F;
/* even sample: fetch new ADPCM data byte, get most-significant nybble */
			else
			{
				temp = fgetc(info->infile);
				if(temp == EOF)
					return -1;
				info->bytes_left--;
				adpcm_info->byte = temp;
				nybble = (temp >> 4) & 0x0F;
			}
			do_adpcm(left_info, left, nybble);
			*right = *left;
		}
		else
		{
			temp = fgetc(info->infile);
			if(temp == EOF)
				return -1;
			info->bytes_left--;
			nybble = (temp >> 4) & 0x0F;
			do_adpcm(left_info, left, nybble);
			nybble = temp & 0x0F;
			do_adpcm(right_info, right, nybble);
		}
		adpcm_info->samples_left--;
	}
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static int validate(sound_info_t *info)
{
#if 0	/* can't do it -- need WAV_FMT_ALIGN entry for WAV file header */
	long temp;
	int error;

	error = wav_validate(info->infile, info, 2);
	if(error != 0)
		return error;
/* compute samples_per_frame as below... */
	return 0;
}
#else
	unsigned char buffer[WAV_FMT_SIZE];
	unsigned samples_per_frame;
	char *data;
	long temp;

	fseek(info->infile, 0, SEEK_SET);
/* is it a .WAV file? */
	if(fread(buffer, 1, 12, info->infile) != 12)
ERR:		return -1;
	if(memcmp(buffer, "RIFF", 4) || memcmp(buffer + 8, "WAVE", 4))
		return -1;				/* not .WAV */
/* skip to 'fmt ' section and read fmt block */
	do
	{
		if(fread(buffer, 1, 4, info->infile) != 4)
			goto ERR;
	} while(memcmp(buffer, "fmt ", 4));
	if(fread(buffer, 1, WAV_FMT_SIZE, info->infile) != WAV_FMT_SIZE)
		goto ERR;
	if(read_le16(buffer + WAV_FMT_FMT) != 2)
		return -1;				/* not MS-ADPCM */
/* it is a .WAV file and it uses MS-ADPCM coding: get some info */
	info->channels = read_le16(buffer + WAV_FMT_CHAN);	/* stereo or mono? */
	info->rate = read_le32(buffer + WAV_FMT_RATE);	/* sample rate */
	info->depth = read_le16(buffer + WAV_FMT_DEPTH);	/* bits/sample */
/* skip rest of header */
	for(temp = read_le32(buffer) - (WAV_FMT_SIZE - 4); temp; temp--)
		if(fgetc(info->infile) == EOF)
			goto ERR;
/* compute samples_per_frame...
...buffer+WAV_FMT_ALIGN contains frame size in bytes... */
	temp = read_le16(buffer + WAV_FMT_ALIGN);
/* ...subtract 7 bytes for header (one header per channel)... */
	temp -= 7 * info->channels;
/* ...convert to bits... */
	temp <<= 3;
/* ...convert to samples (divide by ADPCM sample depth and channels)... */
	temp = temp / (info->depth * info->channels);
/* ...and add 2 for samp1 and samp2 in the ADPCM frame header */
	samples_per_frame = temp + 2;
/* skip to 'data' section. This also skips those useless
'fact' sections in the .WAV file. */
	for(data="data"; *data != '\0'; )
	{
		temp = fgetc(info->infile);
		if(temp == EOF)
			goto ERR;
		if(temp == *data)
			data++;
	}
/* get length of data */
	if(fread(buffer, 1, 4, info->infile) != 4)
		goto ERR;
	info->bytes_left = read_le32(buffer);
/* use calloc(), so it's zeroed */
	info->fsd = (void *)calloc(1, sizeof(adpcm_t));
	if(info->fsd == NULL)
		return -1;
	((adpcm_t *)(info->fsd))->samples_per_frame = samples_per_frame;
/* */
	info->depth = 16;
/* valid MS-ADPCM */
	return 0;
}
#endif
/*****************************************************************************
*****************************************************************************/
codec_t _wav02 =
{
	".wav Microsoft ADPCM", validate, get_sample,
};
