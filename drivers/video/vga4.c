/* $Id: vga4.c,v 1.3 2003/06/22 15:43:38 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>
#include <kernel/vmm.h>

#include <wchar.h>
#include <string.h>

#include "include/video.h"
#include "include/vgamodes.h"

/*! Physical address of the VGA frame buffer */
extern uint8_t *vga_base;
static uint8_t *vga_base_global = PHYSICAL(0xA0000);
static int maskbit[640], y80[480], xconv[640], startmasks[8], endmasks[8];

void swap_int(int *a, int *b);

uint8_t vga4Dither(int x, int y, colour_t clr)
{
    return (uint8_t) clr;
}

void vga4Close(video_t *vid)
{
}

struct
{
    videomode_t mode;
    const uint8_t *regs;
} vga4_modes[] =
{
    /* cookie,  width,  height, bpp,    bpl,    flags,                  regs */
    { { 0x03,    80,     25,    4,      0,      VIDEO_MODE_TEXT, L"fb_vga", },     mode03h },
    { { 0x0d,   320,    200,    4,      0,      VIDEO_MODE_GRAPHICS, L"fb_vga", }, mode0Dh },
    { { 0x0e,   640,    200,    4,      0,      VIDEO_MODE_GRAPHICS, L"fb_vga", }, mode0Eh },
    { { 0x10,   640,    350,    4,      0,      VIDEO_MODE_GRAPHICS, L"fb_vga", }, mode10h },
    { { 0x12,   640,    480,    4,      0,      VIDEO_MODE_GRAPHICS, L"fb_vga", }, mode12h },

  /*{ { 320,        200,        2,        0,        0x04, },        mode04h },
    { { 640,        200,        1,        0,        0x06, },        mode06h },
    { { 640,        350,        1,        0,        0x0f, },        mode0Fh },
    { { 640,        480,        1,        0,        0x11, },        mode11h },*/
};

int vga4EnumModes(video_t *vid, unsigned index, videomode_t *mode)
{
    if (index < _countof(vga4_modes))
    {
        *mode = vga4_modes[index].mode;
        return index == _countof(vga4_modes) - 1 ? VID_ENUM_STOP : VID_ENUM_CONTINUE;
    }
    else
        return VID_ENUM_ERROR;
}

void vga4PreCalc(void)
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

    for (j = 0; j < 480; j++)
        y80[j] = j * video_mode.bytesPerLine;
    for (j = 0; j < 640; j++)
        xconv[j] = j >> 3;
}

bool vga4SetMode(video_t *vid, videomode_t *mode)
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
		{ 0x80, 0x80, 0x80, 0x00 }, // 7
		{ 0xc0, 0xc0, 0xc0, 0x00 }, // 8
		{ 0xff, 0x00, 0x00, 0x00 }, // 9
		{ 0x00, 0xff, 0x00, 0x00 }, // 10
		{ 0xff, 0xff, 0x00, 0x00 }, // 11
		{ 0x00, 0x00, 0xff, 0x00 }, // 12
		{ 0xff, 0x00, 0xff, 0x00 }, // 13
		{ 0x00, 0xff, 0xff, 0x00 }, // 14
		{ 0xff, 0xff, 0xff, 0x00 }, // 15
	};

    const uint8_t *regs;
    unsigned i;
    //clip_t clip;

    regs = NULL;
    for (i = 0; i < _countof(vga4_modes); i++)
        if (vga4_modes[i].mode.cookie == mode->cookie)
        {
            regs = vga4_modes[i].regs;
            break;
        }

    if (regs == NULL)
        return false;

    //if (video_mode.flags & VIDEO_MODE_TEXT)
        //vid->text_memory = vgaSaveTextMemory();

    vgaWriteRegs(regs);
    video_mode = vga4_modes[i].mode;
    video_mode.bytesPerLine = video_mode.width / 8;
    vga4PreCalc();

    //if (video_mode.flags & VIDEO_MODE_TEXT)
    //{
        //vgaRestoreTextMemory(vid->text_memory);
        //vid->text_memory = NULL;
    //}
    //else

    //clip.num_rects = 0;
    //clip.rects = NULL;
    //vid->vidFillRect(vid, &clip, 0, 0, video_mode.width, video_mode.height, 0);

	vgaStorePalette(vid, palette, 0, _countof(palette));
    return true;
}

void vga4PutPixel(video_t *vid, int x, int y, colour_t clr)
{
    uint8_t *offset;
    volatile uint8_t a;
    uint8_t pix;

    pix = vga4Dither(x, y, clr);
    offset = vga_base_global + xconv[x] + y80[y];

    SpinAcquire(&sem_vga);
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

    SpinRelease(&sem_vga);
}

void vga4GetByte(addr_t offset,
                 uint8_t *b, uint8_t *g,
                 uint8_t *r, uint8_t *i)
{
    SpinAcquire(&sem_vga);
    out16(VGA_GC_INDEX, 0x0304);
    *i = vga_base_global[offset];
    out16(VGA_GC_INDEX, 0x0204);
    *r = vga_base_global[offset];
    out16(VGA_GC_INDEX, 0x0104);
    *g = vga_base_global[offset];
    out16(VGA_GC_INDEX, 0x0004);
    *b = vga_base_global[offset];
    SpinRelease(&sem_vga);
}

colour_t vga4GetPixel(video_t *vid, int x, int y)
{
    uint8_t mask, b, g, r, i;

    vga4GetByte(xconv[x] + y80[y], &b, &g, &r, &i);

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

void vga4HLine(video_t *vid, int x1, int x2, int y, colour_t clr)
{
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

	pix = vga4Dither(x1, y, clr);
    offset = vga_base_global + xconv[x1] + y80[y];

    /* midx = start of middle region */
    midx = (x1 + 7) & -8;
    /* leftpix = number of pixels to left of middle */
    leftpix = midx - x1;

    SpinAcquire(&sem_vga);
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

    SpinRelease(&sem_vga);
}

void vga4BltScreenToScreen(video_t *vid, const rect_t *dest, const rect_t *src)
{
}

void vga4BltScreenToMemory(video_t *vid, void *dest, int dest_pitch, const rect_t *src)
{
	unsigned width, height, x, y, mask, plane;
	uint8_t volatile *vid_ptr, *vid_base;
	uint8_t *dest_ptr, *dest_base;

	if (src->right < src->left)
		return;
	if (src->bottom < src->top)
		return;

	width = src->right - src->left;
	height = src->bottom - src->top;

	memset(dest, 0, dest_pitch * height);

	for (plane = 0; plane < 4; plane++)
	{
		mask = 0x80 >> (src->left & 7);
		dest_base = dest;
		vid_base = vga_base_global + xconv[src->left] + y80[src->top];

		SpinAcquire(&sem_vga);
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
					vid_ptr += video_mode.bytesPerLine;
					dest_ptr += dest_pitch;
				}
			}
			else
			{
				for (y = 0; y < height; y++)
				{
					if (*vid_ptr & mask)
						*dest_ptr |= 1 << (plane + 4);
					vid_ptr += video_mode.bytesPerLine;
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

		SpinRelease(&sem_vga);
	}
}

void vga4BltMemoryToScreen(video_t *vid, const rect_t *dest, const void *src, int src_pitch)
{
	unsigned width, height, x, y, mask;
	uint8_t volatile *vid_ptr, *vid_base, a;
	const uint8_t *src_ptr, *src_base;

	if (dest->right < dest->left)
		return;
	if (dest->bottom < dest->top)
		return;

	width = dest->right - dest->left;
	height = dest->bottom - dest->top;

	SpinAcquire(&sem_vga);

	out16(VGA_GC_INDEX, 0x0205);	// write mode 2
	out(VGA_GC_INDEX, 0x08);		// set bit mask
	out16(VGA_SEQ_INDEX, 0xff02);	// write to all planes
	mask = 0x80 >> (dest->left & 7);
	src_base = src;
	vid_base = vga_base_global + xconv[dest->left] + y80[dest->top];

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
				*vid_ptr = *src_ptr & 0x0f;
				vid_ptr += video_mode.bytesPerLine;
				src_ptr += src_pitch;
			}
		}
		else
		{
			for (y = 0; y < height; y++)
			{
				a = *vid_ptr;
				*vid_ptr = *src_ptr >> 4;
				vid_ptr += video_mode.bytesPerLine;
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
	SpinRelease(&sem_vga);
}

void vga4TextOut(int *x, int *y, wchar_t max_char, uint8_t *font_bitmaps, 
                 uint8_t font_height, const wchar_t *str, size_t len, 
                 colour_t afg, colour_t abg)
{
    int ay;
    const uint8_t *data;
    uint8_t *offset, fg, bg;
    volatile int a;
    unsigned char ch[2];

    fg = vga4Dither(*x, *y, afg);
    bg = vga4Dither(*x, *y, abg);

    if (len == -1)
        len = wcslen(str);

    SpinAcquire(&sem_vga);
    for (; len > 0; str++, len--)
    {
        ch[0] = 0;
        //wcsto437(ch, str, 1);
        ch[0] = (uint8_t) *str;
        ch[1] = '\0';
        if (ch[0] > max_char)
            ch[0] = '?';

        data = font_bitmaps + font_height * ch[0];
        offset = vga_base_global + xconv[*x] + y80[*y];

        for (ay = 0; ay < font_height; ay++)
        {
            if (afg != (colour_t) -1)
            {
                /* draw letter */
                out16(VGA_GC_INDEX, 0x08 | (data[ay] << 8));
                if (fg)
                {
                    out16(VGA_SEQ_INDEX, 0x02 | (fg << 8));
                    a = *offset;
                    *offset = 0xff;
                }

                if (~fg)
                {
                    out16(VGA_SEQ_INDEX, 0x02 | (~fg << 8));
                    a = *offset;
                    *offset = 0;
                }
            }

            if (abg != (colour_t) -1)
            {
                /* draw background */
                out16(VGA_GC_INDEX, 0x08 | (~data[ay] << 8));
                if (bg)
                {
                    out16(VGA_SEQ_INDEX, 0x02 | (bg << 8));
                    a = *offset;
                    *offset = 0xff;
                }

                if (~bg)
                {
                    out16(VGA_SEQ_INDEX, 0x02 | (~bg << 8));
                    a = *offset;
                    *offset = 0;
                }
            }

            offset += video_mode.bytesPerLine;
        }

        *x += 8;
    }

    *y += font_height;
    SpinRelease(&sem_vga);
}

void vga4ScrollUp(int pixels, unsigned y, unsigned height)
{
    uint32_t plane, mask;
    uint8_t *src, *dest;
    unsigned line;

    if (pixels > 0)
    {
        dest = vga_base_global + y * video_mode.bytesPerLine;
        src = dest + pixels * video_mode.bytesPerLine;
    }
    else
    {
        src = vga_base_global + y * video_mode.bytesPerLine;
        dest = src + pixels * video_mode.bytesPerLine;
        pixels = -pixels;
    }

    SpinAcquire(&sem_vga);

    out16(VGA_GC_INDEX, 0xff08);
    height = video_mode.bytesPerLine * (height - pixels);

    for (line = 0; line < height; line += 8 * video_mode.bytesPerLine)
        for (mask = 0x0100, plane = 0; mask < 0x1000; mask <<= 1, plane++)
        {
            out16(VGA_SEQ_INDEX, mask | 0x0002);
            out16(VGA_GC_INDEX, (plane << 8) | 0x0004);
            memmove(dest + line, src + line, video_mode.bytesPerLine * 8);
        }

    SpinRelease(&sem_vga);
}

/*
 *  xxx -- RLE DIB is a really stupid format
 */
void vga4DisplayBitmap(unsigned x, unsigned y, unsigned width, 
                       unsigned height, void *data)
{
    uint8_t *bits, count, pix[2];
    int ax, ay, i;
    unsigned off;

    bits = data;
    off = 0;
    ax = 0;
    ay = height - 1;

    while (ay >= 0)
    {
        if (off & 1)                /* start on a word boundary */
            off++;

        if (bits[off] != 0)
        {
            count = bits[off];
            pix[0] = (bits[off + 1] >> 4) & 0x0f;
            pix[1] = (bits[off + 1] >> 0) & 0x0f;
            for (i = 0; i < count; i++)
            {
                vga4PutPixel(NULL, ax + x, ay + y, pix[i & 1]);
                ax++;
                /*if (ax >= width)
                {
                    ax -= width;
                    ay--;
                }*/
            }

            off += 2;
        }
        else
        {
            count = bits[off + 1];
            if (count == 0)
            {
                ax = 0;
                ay--;
                off += 2;
            }
            else if (count == 1)
            {
                off += 2;
                break;
            }
            else if (count == 2)
            {
                ax += bits[off + 2];
                ay -= bits[off + 3];
                off += 4;
            }
            else
            {
                for (i = 0; i < count; i++)
                {
                    if (i & 1)
                        vga4PutPixel(NULL, ax + x, ay + y, 
                            (bits[off + i / 2 + 2] >> 0) & 0x0f);
                    else
                        vga4PutPixel(NULL, ax + x, ay + y, 
                            (bits[off + i / 2 + 2] >> 4) & 0x0f);

                    ax++;
                    /*if (ax >= width)
                    {
                        ax -= width;
                        ay--;
                    }*/
                }

                off += (count + 1) / 2 + 2;
            }
        }
    }
}

video_t vga4 =
{
    vga4Close,
    vga4EnumModes,
    vga4SetMode,
    vgaStorePalette,
    NULL,           /* movecusor */
    vga4PutPixel,
    vga4GetPixel,
    vga4HLine,
    NULL,          /* vline */
    NULL,          /* line */
    NULL,          /* fillrect */
    //NULL,          /* vga4TextOut */
    NULL,          /* fillpolygon */
	vga4BltScreenToScreen,
	vga4BltScreenToMemory,
	vga4BltMemoryToScreen,
};

video_t *vga4Init(device_config_t *cfg)
{
    if (vga_base == NULL)
    {
        vga_base = VmmMap(0x20000 / PAGE_SIZE, NULL, (void*) 0xa0000,
            NULL, VM_AREA_MAP, VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
        VmmShare(vga_base, L"fb_vga");
        wprintf(L"vga4: VGA frame buffer at %p\n", vga_base);
    }

    return &vga4;
}
