/* $Id: mbstowcs.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <wchar.h>
#include <errno.h>

size_t mbstowcs(wchar_t *wcstr, const char *mbstr, size_t count)
{
	int written, bytes;
	uint8_t *b;
	
	b = (uint8_t*) mbstr;
	for (written = 0; *b && written < count;)
	{
		if ((*b & 0x80) == 0)
			bytes = 1;
		else if ((*b & 0xE0) == 0xC0)
			bytes = 2;
		else if ((*b & 0xF0) == 0xE0)
			bytes = 3;
		else
		{
			errno = EILSEQ;
			return -1;
		}

		switch (bytes)
		{
		case 1:
			*wcstr = (wchar_t) b[0];
			break;

		case 2:
			*wcstr = (b[0] & 0x1f) << 6 | 
				(b[1] & 0x3f);
			break;

		case 3:
			*wcstr = (b[0] & 0x0f) << 12 | 
				(b[1] & 0x3f) << 6 | 
				(b[1] & 0x3f);
			break;
		}

		b += bytes;
		wcstr++;
		written++;
	}

	*wcstr = 0;
	return written;
}
