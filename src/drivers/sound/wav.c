/*****************************************************************************
common routines for .WAV files

.WAV sub-formats:
	1=linear PCM
	2=Microsoft ADPCM
	6=A-law (ITU-T G.711, formerly CCITT G.711)
	7=u-law (ITU-T G.711, formerly CCITT G.711)
	34=True Speech
	49=GSM
	85=MP3

.WAV chunks
"fmt "
"data"	xxx - this code does not handle multiple "data" chunks
"fact"	xxx - this code does not skip more than one "fact" chunk
*****************************************************************************/
#include <string.h> /* memcmp() */
#include "sound.h"
/*****************************************************************************
*****************************************************************************/
int wav_validate(sound_info_t *info, unsigned sub_type)
{
	unsigned char buffer[WAV_FMT_SIZE];
	char *data;
	long temp;

	fseek(info->infile, 0, SEEK_SET);
/* is it a .WAV file? */
	if(fread(buffer, 1, 12, info->infile) != 12)
		return -1;
	if(memcmp(buffer, "RIFF", 4) || memcmp(buffer + 8, "WAVE", 4))
		return -1;
/* skip to 'fmt ' section */
	do
	{
		if(fread(buffer, 1, 4, info->infile) != 4)
			return -1;
	} while (memcmp(buffer, "fmt ", 4) != 0);
/* read info from 'fmt ' section */
	if(fread(buffer, 1, WAV_FMT_SIZE, info->infile) != WAV_FMT_SIZE)
		return -1;
/* 1=linear PCM, 2=ADPCM, 85=MP3, etc. */
	if (read_le16(buffer + WAV_FMT_FMT) != sub_type)
		return -1;		/* not the subtype we want */
	info->channels = read_le16(buffer + WAV_FMT_CHAN);
	info->depth = read_le16(buffer + WAV_FMT_DEPTH);
	info->rate = read_le32(buffer + WAV_FMT_RATE);
/* skip rest of 'fmt ' section */
	for(temp = read_le32(buffer) - (WAV_FMT_SIZE - 4); temp != 0; temp--)
	{
		if(fgetc(info->infile) == EOF)
			return -1;
	}
/* skip to 'data' section. This also skips the useless 'fact' sections. */
	for(data = "data"; *data != '\0'; )
	{
		temp = fgetc(info->infile);
		if(temp == EOF)
			return -1;
		if(temp == *data)
			data++;
	}
/* get length of data.
We do NOT handle .WAV files with multiple 'data' sections. */
	if(fread(buffer, 1, 4, info->infile) != 4)
		return -1;
	info->bytes_left = read_le32(buffer);
/* valid .WAV file */
	return 0;
}
