/* $Id$ */

#include <os/framebuf.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static uint8_t Bpp4ColourToIndex(colour_t colour)
{
	uint8_t red, green, blue, intensity;

	switch (colour)
	{
	case 0x000000: return 0;
	case 0x800000: return 1;
	case 0x008000: return 2;
	case 0x808000: return 3;
	case 0x000080: return 4;
	case 0x800080: return 5;
	case 0x008080: return 6;
	case 0xc0c0c0: return 7;
	case 0x808080: return 8;
	case 0xff0000: return 9;
	case 0x00ff00: return 10;
	case 0xffff00: return 11;
	case 0x0000ff: return 12;
	case 0xff00ff: return 13;
	case 0x00ffff: return 14;
	case 0xffffff: return 15;

	default:
		red = COLOUR_RED(colour) >= 0x80;
		green = COLOUR_GREEN(colour) >= 0x80;
		blue = COLOUR_BLUE(colour) >= 0x80;
		intensity = (COLOUR_RED(colour) > 0xc0) || 
			(COLOUR_GREEN(colour) > 0xc0) || 
			(COLOUR_BLUE(colour) > 0xc0);

		return (intensity << 3) | (blue << 2) | (green << 1) | red;
	}
}


static status_t Bpp4ConvertBitmap1(void *dest, int dest_pitch, 
								   const bitmapdesc_t *src)
{
	const uint8_t *src_ptr, *src_base;
	uint8_t *dest_ptr, *dest_base;
	uint8_t src_mask, fg, bg;
	unsigned x, y;

	bg = Bpp4ColourToIndex(MAKE_COLOUR(
		src->palette[0].red, 
		src->palette[0].green, 
		src->palette[0].blue));
	fg = Bpp4ColourToIndex(MAKE_COLOUR(
		src->palette[1].red, 
		src->palette[1].green, 
		src->palette[1].blue));

	src_base = src->data;
	dest_base = dest;
	for (y = 0; y < src->height; y++)
	{
		src_ptr = src_base;
		src_mask = 0x80;
		dest_ptr = dest_base;

		for (x = 0; x < src->width; x++)
		{
			if ((x & 1) == 0)
			{
				/* pixels 0, 2, 4, 6... */
				if ((*src_ptr & src_mask) == 0)
					*dest_ptr = bg << 4;
				else
					*dest_ptr = fg << 4;
			}
			else
			{
				/* pixels 1, 3, 5, 7... */
				if ((*src_ptr & src_mask) == 0)
					*dest_ptr = (*dest_ptr & 0xf0) | bg;
				else
					*dest_ptr = (*dest_ptr & 0xf0) | fg;
				dest_ptr++;
			}

			src_mask >>= 1;
			if (src_mask == 0)
			{
				src_mask = 0x80;
				src_ptr++;
			}
		}

		src_base += src->pitch;
		dest_base += dest_pitch;
	}

	return 0;
}


static status_t Bpp4ConvertBitmap8(void *dest, int dest_pitch, 
								   const bitmapdesc_t *src)
{
	const uint8_t *src_base;
	uint8_t *dest_ptr, *dest_base;
	unsigned x, y;

	src_base = src->data;
	dest_base = dest;
	for (y = 0; y < src->height; y++)
	{
		dest_ptr = dest_base;

		for (x = 0; x < src->width; x++)
		{
			if ((x & 1) == 0)
			{
				/* pixels 0, 2, 4, 6... */
				*dest_ptr = Bpp4ColourToIndex(src_base[x]) << 4;
			}
			else
			{
				/* pixels 1, 3, 5, 7... */
				*dest_ptr = (*dest_ptr & 0xf0) | Bpp4ColourToIndex(src_base[x]);
				dest_ptr++;
			}
		}

		src_base += src->pitch;
		dest_base += dest_pitch;
	}

	return 0;
}


#define SET_TOP(b, n)		((b) = ((b) & 0x0f) | ((n) << 4))
#define SET_BOTTOM(b, n)	((b) = ((b) & 0xf0) | (n))
#define GET_TOP(b)			(((b) & 0xf0) >> 4)
#define GET_BOTTOM(b)		(((b) & 0x0f))

void Bpp4BltMemoryToMemory(surface_t *surf, 
						   bitmapdesc_t *dest, const rect_t *dest_rect,
						   const bitmapdesc_t *src, const rect_t *src_rect)
{
	const uint8_t *src_ptr, *src_base;
	uint8_t *dest_ptr, *dest_base;
	bool src_even, dest_even;
	unsigned x, y;

	src_base = (const uint8_t*) src->data 
		+ src_rect->top * src->pitch 
		+ src_rect->left / 2;
	dest_base = (uint8_t*) dest->data 
		+ dest_rect->top * dest->pitch 
		+ dest_rect->left / 2;

	/*fbprintf(L"Bpp4BltMemoryToMemory: (%ux%u), (%u,%u,%u,%u), (%ux%u), (%u,%u,%u,%u)\n",
		dest->width, dest->height, 
		dest_rect->left, dest_rect->top, dest_rect->right, dest_rect->bottom, 
		src->width, src->height, 
		src_rect->left, src_rect->top, src_rect->right, src_rect->bottom);*/
	for (y = dest_rect->top; y < dest_rect->bottom; y++)
	{
		src_even = (src_rect->left & 1) == 0;
		dest_even = (dest_rect->left & 1) == 0;
		src_ptr = src_base;
		dest_ptr = dest_base;

		for (x = dest_rect->left; x < dest_rect->right; x++)
		{
			if (src_even)
			{
				if (dest_even)
				{
					SET_TOP(*dest_ptr, GET_TOP(*src_ptr));
				}
				else
				{
					SET_BOTTOM(*dest_ptr, GET_TOP(*src_ptr));
					dest_ptr++;
				}
			}
			else
			{
				if (dest_even)
					SET_TOP(*dest_ptr, GET_BOTTOM(*src_ptr));
				else
				{
					SET_BOTTOM(*dest_ptr, GET_BOTTOM(*src_ptr));
					dest_ptr++;
				}

				src_ptr++;
			}

			src_even = !src_even;
			dest_even = !dest_even;
		}

		src_base += src->pitch;
		dest_base += dest->pitch;
	}
}


static bool Bpp4InitBitmap(void **bitmap, int *pitch, 
						   unsigned width, unsigned height)
{
	*pitch = ((width + 7) & -8) / 2;
	*bitmap = malloc(*pitch * height);
	return bitmap != NULL;
}


status_t Bpp4ConvertBitmap(surface_t *surf, void **dest, int *dest_pitch, 
						   const bitmapdesc_t *src)
{
	if (!Bpp4InitBitmap(dest, dest_pitch, src->width, src->height))
		return errno;

	if (src->bits_per_pixel == 0)
	{
		if (src->palette != NULL)
		{
			uint8_t *dest_base, index;
			unsigned y;

			dest_base = *dest;
			index = Bpp4ColourToIndex(MAKE_COLOUR(
				src->palette[0].red, 
				src->palette[0].green, 
				src->palette[0].blue));
			index |= index << 4;

			for (y = 0; y < src->height; y++)
			{
				memset(dest_base, index, src->width / 2);
				dest_base += *dest_pitch;
			}
		}

		return 0;
	}

	switch (src->bits_per_pixel)
	{
	case 1:
		return Bpp4ConvertBitmap1(*dest, *dest_pitch, src);

	case 8:
		return Bpp4ConvertBitmap8(*dest, *dest_pitch, src);

	default:
		free(*dest);
		*dest = NULL;
		return ENOTIMPL;
	}
}
