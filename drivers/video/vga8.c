/* $Id: vga8.c,v 1.1 2002/12/21 09:49:07 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/arch.h>
#include <kernel/vmm.h>

#include <wchar.h>

#include "video.h"
#include "vgamodes.h"
#include "bpp8.h"

static uint8_t *video_base;

void swap_int(int *a, int *b);

static void vga8Close(video_t *vid)
{
}

static struct
{
    videomode_t mode;
    const uint8_t *regs;
} vga8_modes[] =
{
    /*  cookie, width,  height, bpp, bpl,   flags,               framebuffer,   regs */
    { { 0x13,   320,    200,    8,   0,     VIDEO_MODE_GRAPHICS, L"fb_vga", },	mode13h },
};

static int vga8EnumModes(video_t *vid, unsigned index, videomode_t *mode)
{
    if (index < _countof(vga8_modes))
    {
        vga8_modes[index].mode.bytesPerLine = 
            (vga8_modes[index].mode.width * vga8_modes[index].mode.bitsPerPixel) / 8;
        *mode = vga8_modes[index].mode;
        return index == _countof(vga8_modes) - 1 ? VID_ENUM_STOP : VID_ENUM_CONTINUE;
    }
    else
        return VID_ENUM_ERROR;
}

static bool vga8SetMode(video_t *vid, videomode_t *mode)
{
    const uint8_t *regs;
    unsigned i;
    clip_t clip;
    rect_t rect;

    regs = NULL;
    for (i = 0; i < _countof(vga8_modes); i++)
        if (vga8_modes[i].mode.cookie == mode->cookie)
        {
            regs = vga8_modes[i].regs;
            break;
        }

    if (regs == NULL)
        return false;

    //if (video_mode.flags & VIDEO_MODE_TEXT)
    //vid->text_memory = vgaSaveTextMemory();

    video_mode = vga8_modes[i].mode;
    vgaWriteRegs(regs);

    //if (video_mode.flags & VIDEO_MODE_TEXT)
    //{
    //vgaRestoreTextMemory(vid->text_memory);
    //vid->text_memory = NULL;
    //}
    //else
    /*rect.left = rect.top = 0;
    rect.right = video_mode.width;
    rect.bottom = video_mode.height;
    clip.num_rects = 1;
    clip.rects = &rect;
    vid->vidFillRect(vid, &clip, 0, 0, video_mode.width, video_mode.height, 0);*/

    bpp8GeneratePalette(bpp8_palette);
    vgaStorePalette(vid, bpp8_palette, 0, _countof(bpp8_palette));
    return true;
}

static void vga8PutPixel(video_t *vid, const clip_t *clip, int x, int y, 
                         colour_t clr)
{
    unsigned i;

    for (i = 0; i < clip->num_rects; i++)
        if (x >= clip->rects[i].left &&
            y >= clip->rects[i].top &&
            x <  clip->rects[i].right &&
            y <  clip->rects[i].bottom)
        {
            video_base[x + y * video_mode.bytesPerLine] = 
                bpp8Dither(x, y, clr);
            break;
        }
}

static colour_t vga8GetPixel(video_t *vid, int x, int y)
{
    uint8_t index;
    index = video_base[x + y * video_mode.bytesPerLine];
    return MAKE_COLOUR(bpp8_palette[index].red, 
        bpp8_palette[index].green, 
        bpp8_palette[index].blue);
}

static void vga8HLine(video_t *vid, const clip_t *clip, int x1, int x2, int y, 
                      colour_t clr)
{
    uint8_t *ptr;
    unsigned i;
    int ax1, ax2;

    if (x2 < x1)
        swap_int(&x1, &x2);

    for (i = 0; i < clip->num_rects; i++)
    {
        if (y >= clip->rects[i].top && y < clip->rects[i].bottom)
        {
            ax1 = max(clip->rects[i].left, x1);
            ax2 = min(clip->rects[i].right, x2);
            for (ptr = video_base + ax1 + y * video_mode.bytesPerLine;
            ax1 < ax2;
            ax1++, ptr++)
                *ptr = bpp8Dither(ax1, y, clr);
        }
    }
}

#if 0
static void vga8TextOut(video_t *vid, 
                        int *x, int *y, vga_font_t *font, const wchar_t *str, 
                        size_t len, colour_t afg, colour_t abg)
{
    int ay, i;
    uint8_t *data, fg, bg, *offset;
    unsigned char ch[2];

    fg = bpp8Dither(*x, *y, afg);
    bg = bpp8Dither(*x, *y, abg);

    if (len == -1)
        len = wcslen(str);

    SemAcquire(&sem_vga);
    for (; len > 0; str++, len--)
    {
        ch[0] = 0;
        wcsto437(ch, str, 1);
        if (ch[0] < font->First || ch[0] > font->Last)
            ch[0] = '?';

        data = font->Bitmaps + font->Height * (ch[0] - font->First);
        offset = video_base + *x + *y * video_mode.bytesPerLine;

        for (ay = 0; ay < font->Height; ay++)
        {
            if (afg != (colour_t) -1)
                for (i = 0; i < 8; i++)
                    if (data[ay] & (1 << i))
                        offset[8 - i] = fg;

                    if (abg != (colour_t) -1)
                        for (i = 0; i < 8; i++)
                            if ((data[ay] & (1 << i)) == 0)
                                offset[8 - i] = bg;

                            offset += video_mode.bytesPerLine;
        }

        *x += 8;
    }

    *y += font->Height;
    SemRelease(&sem_vga);
}
#endif

static video_t vga8 =
{
    vga8Close,
    vga8EnumModes,
    vga8SetMode,
    vgaStorePalette,
    NULL,           /* movecursor */
    vga8PutPixel,
    vga8GetPixel,
    vga8HLine,
    NULL,	   /* vline */
    NULL,	   /* line */
    NULL,	   /* fillrect */
    NULL, //vga8TextOut,
    NULL,	   /* fillpolygon */
};

video_t *vga8Init(device_config_t *cfg)
{
    if (video_base == NULL)
    {
        video_base = VmmMap(0x20000 / PAGE_SIZE, NULL, (void*) 0xa0000,
            NULL, VM_AREA_MAP, VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
        VmmShare(video_base, L"fb_vga");
        wprintf(L"vga8: VGA frame buffer at %p\n", video_base);
    }

    return &vga8;
}
