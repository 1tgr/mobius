/* $Id$ */

#include <assert.h>
#include <stdio.h>
#include <stddef.h>

#include "multibyte.h"

void ConInitMbState(con_mbstate_t *state)
{
	state->spare[0] = state->spare[1] = '\0';
	state->num_spare = 0;
}


bool ConIsLeadByte(char ch)
{
	return ((ch & 0x80) == 0) ||
		((ch & 0xE0) == 0xC0) ||
		((ch & 0xF0) == 0xE0);
}


size_t ConMultiByteToWideChar(wchar_t *dest, const char *src, size_t src_bytes, con_mbstate_t *state)
{
	int i;
	unsigned char b[3];
	unsigned count;
	size_t dest_chars;

	if (state->num_spare > 0)
		assert(ConIsLeadByte(state->spare[0]));

	switch (state->num_spare)
	{
	case 0:
		if (src_bytes > 0)
			b[0] = src[0];
		if (src_bytes > 1)
			b[1] = src[1];
		if (src_bytes > 2)
			b[2] = src[2];
		break;

	case 1:
		b[0] = state->spare[0];
		if (src_bytes > 0)
			b[1] = src[0];
		if (src_bytes > 1)
			b[2] = src[1];
		break;

	case 2:
		b[0] = state->spare[0];
		b[1] = state->spare[1];
		if (src_bytes > 0)
			b[2] = src[0];
		break;
	}

	dest_chars = 0;
	i = -state->num_spare;
	state->num_spare = 0;
	while (i < (int) src_bytes)
	{
		if ((b[0] & 0x80) == 0)
			count = 1;
		else if ((b[0] & 0xE0) == 0xC0)
			count = 2;
		else if ((b[0] & 0xF0) == 0xE0)
			count = 3;
		else
		{
			_wdprintf(L"ConMultiByteToWideChar: invalid lead byte: %02x = %c\n",
				b[0], b[0]);
			count = 1;
		}

		if (i + count > src_bytes)
		{
			int j;

			state->num_spare = (int) src_bytes - i;
			assert(state->num_spare <= _countof(state->spare));
			for (j = 0; j < state->num_spare; j++)
				state->spare[j] = src[i + j];

			break;
		}

		i += count;

		switch (count)
        {
        case 1:
            *dest = (wchar_t) b[0];
			b[0] = b[1];
			b[1] = b[2];
			b[2] = src[i + 2];
            break;

        case 2:
            *dest = (b[0] & 0x1f) << 6 | 
                (b[1] & 0x3f);
			b[0] = b[2];
			b[1] = src[i + 1];
			b[2] = src[i + 2];
            break;

        case 3:
            *dest = (b[0] & 0x0f) << 12 | 
                (b[1] & 0x3f) << 6 | 
                (b[2] & 0x3f);
			b[0] = src[i + 0];
			b[1] = src[i + 1];
			b[2] = src[i + 2];
            break;
        }

		dest_chars++;
		dest++;
	}

	return dest_chars;
}
