/* $Id: vga4.c,v 1.3 2003/06/22 15:43:38 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>
#include <kernel/vmm.h>

#include <wchar.h>
#include <string.h>

#include "video.h"
#include "vgamodes.h"
#include "vga.h"


typedef struct vga4_surf_t vga4_surf_t;
struct vga4_surf_t
{
	int bytes_per_line;
	int y80[480];
};


extern uint8_t *const vga_base_global;

static int maskbit[640], xconv[640], startmasks[8], endmasks[8];

static uint8_t Vga4Dither(int x, int y, colour_t colour)
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


static void Vga4PutPixel(surface_t *surf, int x, int y, colour_t clr)
{
	vga4_surf_t *v4surf;
    uint8_t *offset;
    volatile uint8_t a;
    uint8_t pix;

	v4surf = surf->cookie;
    pix = Vga4Dither(x, y, clr);
    offset = vga_base_global + xconv[x] + v4surf->y80[y];

    VidLockVga();
    out16(VGA_GC_INDEX, 0x08 | (maskbit[x] << 8));
    if (pix)
    {
        out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
        a = *offset;
        *offset = 0xff;
    }

    if (~pix)
    {
        out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
        a = *offset;
        *offset = 0;
    }

    VidUnlockVga();
}


static void Vga4GetByte(addr_t offset,
						uint8_t *b, uint8_t *g,
						uint8_t *r, uint8_t *i)
{
    VidLockVga();
    out16(VGA_GC_INDEX, 0x0304);
    *i = vga_base_global[offset];
    out16(VGA_GC_INDEX, 0x0204);
    *r = vga_base_global[offset];
    out16(VGA_GC_INDEX, 0x0104);
    *g = vga_base_global[offset];
    out16(VGA_GC_INDEX, 0x0004);
    *b = vga_base_global[offset];
    VidUnlockVga();
}


static colour_t Vga4GetPixel(surface_t *surf, int x, int y)
{
	vga4_surf_t *v4surf;
    uint8_t mask, b, g, r, i;

	v4surf = surf->cookie;
    Vga4GetByte(xconv[x] + v4surf->y80[y], &b, &g, &r, &i);

    mask = maskbit[x];
    b &= mask;
    g &= mask;
    r &= mask;
    i &= mask;

    mask = 7 - (x % 8);
    g >>= mask;
    b >>= mask;
    r >>= mask;
    i >>= mask;

    return b + 2 * g + 4 * r + 8 * i;
}


static void Vga4HLine(surface_t *surf, int x1, int x2, int y, colour_t clr)
{
	vga4_surf_t *v4surf;
    int midx, leftpix, rightx, midpix, rightpix;
    uint8_t leftmask, rightmask, pix, *offset;
    volatile uint8_t a;

    if (x2 < x1)
    {
        int temp;
        temp = x2;
        x2 = x1;
        x1 = temp;
    }

	v4surf = surf->cookie;
	pix = Vga4Dither(x1, y, clr);
    offset = vga_base_global + xconv[x1] + v4surf->y80[y];

    /* midx = start of middle region */
    midx = (x1 + 7) & -8;
    /* leftpix = number of pixels to left of middle */
    leftpix = midx - x1;

    VidLockVga();
    if (leftpix > 0)
    {
        /* leftmask = pixels set to left of middle */
        leftmask = 0xff >> (8 - leftpix);
        /*leftmask = startmasks[leftpix];*/

        out16(VGA_GC_INDEX, 0x08 | (leftmask << 8));
        out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
        a = *offset;
        *offset = 0xff;
        out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
        a = *offset;
        *offset = 0;

        offset++;
    }

    /* rightx = end of middle region */
    rightx = x2 & -8;
    /* midpix = number of pixels in middle */
    if (rightx > midx)
    {
        midpix = rightx - midx;
        out16(VGA_GC_INDEX, 0xff08);
        if (pix)
        {
            out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
            memset(offset, 0xff, midpix / 8);
        }

        if (~pix)
        {
            out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
            memset(offset, 0, midpix / 8);
        }

        offset += midpix / 8;
    }

    /* rightpix = number of pixels to right of middle */
    rightpix = x2 - rightx;
    if (rightpix > 0)
    {
        /* rightmask = pixels set to right of middle */
        rightmask = 0xff << (8 - rightpix);
        /*rightmask = endmasks[rightpix];*/

        out16(VGA_GC_INDEX, 0x08 | (rightmask << 8));
        out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
        a = *offset;
        *offset = 0xff;
        out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
        a = *offset;
        *offset = 0;
    }

    VidUnlockVga();
}


static void Vga4FillRect(surface_t *surf, int x1, int y1, int x2, int y2, colour_t c)
{
    vga4_surf_t *v4surf;
    int midx, leftpix, rightx, midpix, rightpix, y;
    uint8_t leftmask, rightmask, pix, *vid_base, *vid_ptr;
    volatile uint8_t a;

    if (x2 < x1)
    {
        int temp;
        temp = x2;
        x2 = x1;
        x1 = temp;
    }

	v4surf = surf->cookie;
	pix = Vga4Dither(x1, y1, c);

    /* midx = start of middle region */
    midx = (x1 + 7) & -8;
    /* leftpix = number of pixels to left of middle */
    leftpix = midx - x1;
	vid_base = vga_base_global + xconv[x1] + v4surf->y80[y1];

    VidLockVga();
	vid_ptr = vid_base;
    if (leftpix > 0)
    {
        /* leftmask = pixels set to left of middle */
        leftmask = 0xff >> (8 - leftpix);

        out16(VGA_GC_INDEX, 0x08 | (leftmask << 8));

		for (y = y1; y < y2; y++)
		{
			out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
			a = *vid_ptr;
			*vid_ptr = 0xff;
			out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
			a = *vid_ptr;
			*vid_ptr = 0;

			vid_ptr += v4surf->bytes_per_line;
		}

		vid_base++;
    }

    /* rightx = end of middle region */
    rightx = x2 & -8;
    /* midpix = number of pixels in middle */
	vid_ptr = vid_base;
    if (rightx > midx)
    {
        midpix = rightx - midx;

		for (y = y1; y < y2; y++)
		{
			out16(VGA_GC_INDEX, 0xff08);
			if (pix)
			{
				out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
				memset(vid_ptr, 0xff, midpix / 8);
			}

			if (~pix)
			{
				out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
				memset(vid_ptr, 0, midpix / 8);
			}

			vid_ptr += v4surf->bytes_per_line;
		}

        vid_base += midpix / 8;
    }

    /* rightpix = number of pixels to right of middle */
    rightpix = x2 - rightx;
	vid_ptr = vid_base;
    if (rightpix > 0)
    {
        /* rightmask = pixels set to right of middle */
        rightmask = 0xff << (8 - rightpix);

		for (y = y1; y < y2; y++)
		{
			out16(VGA_GC_INDEX, 0x08 | (rightmask << 8));
			out16(VGA_SEQ_INDEX, 0x02 | (pix << 8));
			a = *vid_ptr;
			*vid_ptr = 0xff;
			out16(VGA_SEQ_INDEX, 0x02 | (~pix << 8));
			a = *vid_ptr;
			*vid_ptr = 0;

			vid_ptr += v4surf->bytes_per_line;
		}
    }

    VidUnlockVga();
}


static void Vga4BltScreenToScreen(surface_t *surf, const rect_t *dest, const rect_t *src)
{
	vga4_surf_t *v4surf;
	unsigned height, x, y, plane, src_leftpix, dest_leftpix, middle_bytes;
	uint8_t *src_ptr, *src_base, *dest_ptr, *dest_base;
	uint16_t leftmask, rightmask;
	rect_t dest8, src8;

	if (src->right < src->left)
		return;
	if (src->bottom < src->top)
		return;

	v4surf = surf->cookie;

	height = src->bottom - src->top;

	dest8 = *dest;
	dest8.left = (dest8.left + 7) & -8;
	dest8.right = dest8.right & -8;

	src8 = *src;
	src8.left = (src8.left + 7) & -8;
	src8.right = src8.right & -8;

	src_leftpix = src8.left - src->left;
	dest_leftpix = dest8.left - dest->left;

	leftmask = 0xff00 >> (8 - dest_leftpix);
	leftmask &= 0xff00;
	rightmask = 0xff00 << (8 - (dest->right - dest8.right));

	middle_bytes = (src8.right - src8.left) / 8;

	dest_base = vga_base_global + xconv[dest->left] + v4surf->y80[dest->top];
	src_base = vga_base_global + xconv[src->left] + v4surf->y80[src->top];

	for (plane = 0; plane < 4; plane++)
	{
		VidLockVga();
		out16(VGA_GC_INDEX, (plane << 8) | 0x0004);			// select read plane
		out16(VGA_GC_INDEX, 0x0105);						// write mode 1
		out16(VGA_SEQ_INDEX, (0x0100 << plane) | 0x0002);	// select write plane

		dest_ptr = dest_base;
		src_ptr = src_base;
		if (dest8.left != dest->left)
		{
			out16(VGA_GC_INDEX, leftmask | 0x0008);			// set left mask
			for (y = 0; y < height; y++)
			{
				*dest_ptr = *src_ptr;
				dest_ptr += v4surf->bytes_per_line;
				src_ptr += v4surf->bytes_per_line;
			}

			dest_ptr = dest_base + 1;
			src_ptr = src_base + 1;
		}

		out16(VGA_GC_INDEX, 0xff08);						// set middle mask
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < middle_bytes; x++)
				dest_ptr[x] = src_ptr[x];
			dest_ptr += v4surf->bytes_per_line;
			src_ptr += v4surf->bytes_per_line;
		}

		if (dest8.right != dest->right)
		{
			out16(VGA_GC_INDEX, rightmask | 0x0008);		// set right mask
			dest_ptr = vga_base_global + xconv[dest8.right] + v4surf->y80[dest8.top];
			src_ptr = vga_base_global + xconv[src8.right] + v4surf->y80[src8.top];
			for (y = 0; y < height; y++)
			{
				*dest_ptr = *src_ptr;
				dest_ptr += v4surf->bytes_per_line;
				src_ptr += v4surf->bytes_per_line;
			}
		}

		VidUnlockVga();
	}
}


void Vga4BltScreenToMemory(surface_t *surf, void *dest, int dest_pitch, const rect_t *src)
{
	vga4_surf_t *v4surf;
	unsigned width, height, x, y, mask, plane;
	uint8_t volatile *vid_ptr, *vid_base;
	uint8_t *dest_ptr, *dest_base;

	if (src->right < src->left)
		return;
	if (src->bottom < src->top)
		return;

	v4surf = surf->cookie;
	width = src->right - src->left;
	height = src->bottom - src->top;

	memset(dest, 0, dest_pitch * height);

	VidLockVga();
	for (plane = 0; plane < 4; plane++)
	{
		mask = 0x80 >> (src->left & 7);
		dest_base = dest;
		vid_base = vga_base_global + xconv[src->left] + v4surf->y80[src->top];

		out16(VGA_GC_INDEX, (plane << 8) | 0x0004);

		for (x = 0; x < width; x++)	
		{
			dest_ptr = dest_base;
			vid_ptr = vid_base;

			if ((x & 1) == 0)
			{
				for (y = 0; y < height; y++)
				{
					if (*vid_ptr & mask)
						*dest_ptr |= 1 << plane;
					vid_ptr += v4surf->bytes_per_line;
					dest_ptr += dest_pitch;
				}
			}
			else
			{
				for (y = 0; y < height; y++)
				{
					if (*vid_ptr & mask)
						*dest_ptr |= 1 << (plane + 4);
					vid_ptr += v4surf->bytes_per_line;
					dest_ptr += dest_pitch;
				}

				dest_base++;
			}

			mask >>= 1;
			if (mask == 0)
			{
				mask = 0x80;
				vid_base++;
			}
		}
	}

	VidUnlockVga();
}


void Vga4BltMemoryToScreen(surface_t *surf, const rect_t *dest, const void *src, int src_pitch)
{
	vga4_surf_t *v4surf;
	unsigned width, height, x, y, mask;
	uint8_t volatile *vid_ptr, *vid_base, a;
	const uint8_t *src_ptr, *src_base;

	if (dest->right < dest->left)
		return;
	if (dest->bottom < dest->top)
		return;

	v4surf = surf->cookie;
	width = dest->right - dest->left;
	height = dest->bottom - dest->top;

	VidLockVga();

	out16(VGA_GC_INDEX, 0x0205);	// write mode 2
	out(VGA_GC_INDEX, 0x08);		// set bit mask
	out16(VGA_SEQ_INDEX, 0xff02);	// write to all planes
	mask = 0x80 >> (dest->left & 7);
	src_base = src;
	vid_base = vga_base_global + xconv[dest->left] + v4surf->y80[dest->top];

	for (x = 0; x < width; x++)
	{
		src_ptr = src_base;
		vid_ptr = vid_base;

		out(VGA_GC_DATA, mask);

		if ((x & 1) == 0)
		{
			for (y = 0; y < height; y++)
			{
				a = *vid_ptr;
				*vid_ptr = *src_ptr >> 4;
				vid_ptr += v4surf->bytes_per_line;
				src_ptr += src_pitch;
			}
		}
		else
		{
			for (y = 0; y < height; y++)
			{
				a = *vid_ptr;
				*vid_ptr = *src_ptr & 0x0f;
				vid_ptr += v4surf->bytes_per_line;
				src_ptr += src_pitch;
			}

			src_base++;
		}

		mask >>= 1;
		if (mask == 0)
		{
			mask = 0x80;
			vid_base++;
		}
	}

	out16(VGA_GC_INDEX, 0x0005);	// write mode 0
	VidUnlockVga();
}

static const surface_vtbl_t vga4_vtbl =
{
	Vga4PutPixel,
	Vga4GetPixel,
	Vga4HLine,
	SurfVLine,
	SurfLine,
	Vga4FillRect,
	Vga4BltScreenToScreen,
	Vga4BltScreenToMemory,
	Vga4BltMemoryToScreen,
};

void Vga4Init(void)
{
    unsigned long j;

    startmasks[7] = 255;
    startmasks[6] = 127;
    startmasks[5] = 63;
    startmasks[4] = 31;
    startmasks[3] = 15;
    startmasks[2] = 7;
    startmasks[1] = 3;
    startmasks[0] = 1;

    endmasks[0] = 255;
    endmasks[1] = 128;
    endmasks[2] = 192;
    endmasks[3] = 224;
    endmasks[4] = 240;
    endmasks[5] = 248;
    endmasks[6] = 252;
    endmasks[7] = 254;

    for (j = 0; j < 80; j++)
    {
        maskbit[j * 8] = 128;
        maskbit[j * 8 + 1] = 64;
        maskbit[j * 8 + 2] = 32;
        maskbit[j * 8 + 3] = 16;
        maskbit[j * 8 + 4] = 8;
        maskbit[j * 8 + 5] = 4;
        maskbit[j * 8 + 6] = 2;
        maskbit[j * 8 + 7] = 1;
    }

    for (j = 0; j < 640; j++)
        xconv[j] = j >> 3;
}


bool Vga4EnterMode(void *cookie, videomode_t *mode, uint8_t *regs, 
				   const surface_vtbl_t **surf_vtbl, void **surf_cookie)
{
	static const rgb_t palette[] =
	{
		{ 0x00, 0x00, 0x00, 0x00 }, // 0
		{ 0x80, 0x00, 0x00, 0x00 }, // 1
		{ 0x00, 0x80, 0x00, 0x00 }, // 2
		{ 0x80, 0x80, 0x00, 0x00 }, // 3
		{ 0x00, 0x00, 0x80, 0x00 }, // 4
		{ 0x80, 0x00, 0x80, 0x00 }, // 5
		{ 0x00, 0x80, 0x80, 0x00 }, // 6
		{ 0xc0, 0xc0, 0xc0, 0x00 }, // 8
		{ 0x80, 0x80, 0x80, 0x00 }, // 7
		{ 0xff, 0x00, 0x00, 0x00 }, // 9
		{ 0x00, 0xff, 0x00, 0x00 }, // 10
		{ 0xff, 0xff, 0x00, 0x00 }, // 11
		{ 0x00, 0x00, 0xff, 0x00 }, // 12
		{ 0xff, 0x00, 0xff, 0x00 }, // 13
		{ 0x00, 0xff, 0xff, 0x00 }, // 14
		{ 0xff, 0xff, 0xff, 0x00 }, // 15
	};

    unsigned i;
	vga4_surf_t *v4surf;

	v4surf = malloc(sizeof(*v4surf));
	if (v4surf == NULL)
		return false;

    VidWriteVgaRegisters(regs);

	v4surf->bytes_per_line = mode->bytesPerLine;
	for (i = 0; i < 480; i++)
        v4surf->y80[i] = i * 80;

	VidStoreVgaPalette(cookie, palette, 0, _countof(palette));

	*surf_vtbl = &vga4_vtbl;
	*surf_cookie = v4surf;
    return true;
}
