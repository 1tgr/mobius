/*****************************************************************************
common routines for .au files

.au sub-formats:
	1=8-bit u-law
	2=8-bit linear
	3=16-bit linear
	27=A-law (not implemented)
*****************************************************************************/
#include <string.h>	/* memcmp() */
#include <stdio.h> 	/* FILE, EOF, fread(), fgetc() */
#include "sound.h"	/* soundinfo */

/* byte offsets in .au file header */
#define	AU_HDR_MAGIC	0
#define	AU_HDR_HDRLEN	4
#define	AU_HDR_DATALEN	8
#define	AU_HDR_FMT	12
#define	AU_HDR_RATE	16
#define	AU_HDR_CHAN	20
#define	AU_HDR_SIZE	24
/*****************************************************************************
*****************************************************************************/
int au_validate(sound_info_t *info, unsigned sub_type)
{
	unsigned char buffer[AU_HDR_SIZE];
	unsigned long hdr_len;

	fseek(info->infile, 0, SEEK_SET);
	if(fread(buffer, 1, AU_HDR_SIZE, info->infile) != AU_HDR_SIZE)
		return -1;
	if(memcmp(buffer + AU_HDR_MAGIC, ".snd", 4))
		return -1;
/* get data and header lengths */
	hdr_len = read_be32(buffer + AU_HDR_HDRLEN);
	info->bytes_left = read_be32(buffer + AU_HDR_DATALEN);
/* coding */
	if(read_be32(buffer + AU_HDR_FMT) != sub_type)
		return -1;
	info->rate = read_be32(buffer + AU_HDR_RATE);
	info->channels = read_be32(buffer + AU_HDR_CHAN);
/* skip rest of header */
	for(hdr_len -= AU_HDR_SIZE; hdr_len != 0; hdr_len--)
	{
		if(fgetc(info->infile) == EOF)
			return -1;
	}
	return 0;
}
