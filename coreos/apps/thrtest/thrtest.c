/*
 * $Header: /coreos/apps/thrtest/thrtest.c 1     13/03/04 16:11 Tim $
 */

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>

#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <os/framebuf.h>
#include <os/keyboard.h>


typedef struct bounce_t bounce_t;
struct bounce_t
{
    int x;
    int y;
    int dx;
    int dy;
    colour_t colour;
    wchar_t name[16];
};

#define WIDTH   70
#define HEIGHT  20


bounce_t bounces[256];
unsigned num_bounces;

surface_t screen, back, *draw;
videomode_t mode;
font_t *font;
bool quit;
void *back_buffer;
int back_pitch;
lmutex_t mux_screen;
lmutex_t mux_bounces;

int BounceThread(void *param)
{
    bounce_t *b;

    b = param;

    while (!quit)
    {
        rect_t dest, old;
        int new_x, new_y;

        LmuxAcquire(&mux_bounces);

        old.left = b->x;
        old.top = b->y;
        old.right = old.left + WIDTH;
        old.bottom = old.top + HEIGHT;

        if (old.left == 0 ||
            old.right >= mode.width)
            b->dx = -b->dx;

        if (old.top == 0 ||
            old.bottom >= mode.height)
            b->dy = -b->dy;

        new_x = old.left + b->dx;
        new_y = old.top + b->dy;
        b->x = new_x;
        b->y = new_y;

        LmuxRelease(&mux_bounces);
        LmuxAcquire(&mux_screen);
        draw->vtbl->SurfFillRect(draw, old.left, old.top, old.left + WIDTH, old.top + HEIGHT, 0xffffff);
        draw->vtbl->SurfFillRect(draw, new_x, new_y, new_x + WIDTH, new_y + HEIGHT, b->colour);
        if (font != NULL)
            FontDrawText(font, 
                draw, 
                new_x, new_y, 
                b->name, 
                wcslen(b->name),
                0, b->colour);

        draw->vtbl->SurfFlush(draw);

        dest.left = min(old.left, new_x);
        dest.top = min(old.top, new_y);
        dest.right = max(old.left, new_x) + WIDTH;
        dest.bottom = max(old.top, new_y) + HEIGHT;

        screen.vtbl->SurfBltMemoryToScreen(&screen, 
            &dest, 
            (uint8_t*) back_buffer + min(old.top, new_y) * back_pitch + (min(old.left, new_x) * mode.bitsPerPixel) / 8, 
            back_pitch);
        LmuxRelease(&mux_screen);
    }

    return 0;
}


handle_t VidInit(videomode_t *new_mode, videomode_t *old_mode)
{
    handle_t video;
    params_vid_t params;

    video = FsOpen(SYS_DEVICES L"/Classes/video0", 0);
    if (video == 0)
        goto err0;

    if (old_mode != NULL)
    {
        if (!FsRequestSync(video, VID_GETMODE, &params, sizeof(params), NULL))
            goto err1;

        *old_mode = params.vid_setmode;
    }

    params.vid_setmode = *new_mode;
    if (!FsRequestSync(video, VID_SETMODE, &params, sizeof(params), NULL))
        goto err1;

    *new_mode = params.vid_setmode;
    return video;

err1:
    HndClose(video);
err0:
    return 0;
}


void VidCleanup(handle_t video, const videomode_t *old_mode)
{
    params_vid_t params;

    if (old_mode != NULL)
    {
        params.vid_setmode = *old_mode;
        FsRequestSync(video, VID_SETMODE, &params, sizeof(params), NULL);
    }

    HndClose(video);
}


bool SurfCreateBitmap(surface_t *surf, unsigned width, unsigned height, 
                      rgb_t *palette, bitmapdesc_t *bitmap)
{
    status_t status;
    void *data;
    int pitch;

    bitmap->width = width;
    bitmap->height = height;
    bitmap->bits_per_pixel = 0;
    bitmap->pitch = 0;
    bitmap->data = NULL;
    bitmap->palette = palette;
    status = surf->vtbl->SurfConvertBitmap(surf, &data, &pitch, bitmap);
    if (status != 0)
    {
        errno = status;
        return false;
    }

    bitmap->data = data;
    bitmap->pitch = pitch;
    return true;
}


void SurfDeleteBitmap(bitmapdesc_t *bitmap)
{
    free(bitmap->data);
    bitmap->data = NULL;
}


void WaitForKeyPress(void)
{
    uint32_t key;

    while (FsRead(ProcGetProcessInfo()->std_in,
        &key,
        0,
        sizeof(key),
        NULL) && 
        (key & KBD_BUCKY_RELEASE) != 0)
        ;
}


int main(int argc, char **argv)
{
    unsigned i;
    videomode_t old_mode, back_mode;
    handle_t video, threads[_countof(bounces)];
    void *base;
    bitmapdesc_t back_bitmap;

    if (argc == 1)
        num_bounces = 5;
    else
    {
        num_bounces = strtoul(argv[1], NULL, 0);
        if (num_bounces > _countof(threads))
            num_bounces = _countof(threads);
    }

    mode.bitsPerPixel = 8;
    video = VidInit(&mode, &old_mode);
    if (video == 0)
    {
        _wdprintf(L"%S: VidInit\n", argv[0]);
        goto err0;
    }

    if (mode.framebuffer[0] == '\0')
    {
        if (!AccelCreateSurface(&mode, video, &screen))
        {
            _wdprintf(L"%S: AccelCreateSurface\n", argv[0]);
            goto err1;
        }
    }
    else
    {
        handle_t memory;

        memory = HndOpen(mode.framebuffer);
	base = VmmMapSharedArea(memory,
            NULL,
            VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
	HndClose(memory);

	if (!FramebufCreateSurface(&mode, base, &screen))
        {
            _wdprintf(L"%S: FramebufCreateSurface (screen)\n", argv[0]);
            goto err1;
        }
    }

    if (!FontInit())
    {
        goto err2;
        _wdprintf(L"%S: FontInit\n", argv[0]);
    }

    font = FontLoad(L"/mobius/vera.ttf", 
        10 * 64, 
        (mode.bitsPerPixel < 8) ? FB_FONT_MONO : FB_FONT_SMOOTH);

    if (!SurfCreateBitmap(&screen, mode.width, mode.height, NULL, &back_bitmap))
    {
        _wdprintf(L"%S: SurfCreateBitmap\n", argv[0]);
        goto err3;
    }

    back_buffer = back_bitmap.data;
    back_pitch = back_bitmap.pitch;

    memset(&back_mode, 0, sizeof(back_mode));
    back_mode.width = back_bitmap.width;
    back_mode.height = back_bitmap.height;
    back_mode.bitsPerPixel = mode.bitsPerPixel;
    back_mode.bytesPerLine = back_bitmap.pitch;
    if (!FramebufCreateSurface(&back_mode, back_bitmap.data, &back))
    {
        _wdprintf(L"%S: FramebufCreateSurface (back buffer)\n", argv[0]);
        goto err4;
    }

    draw = &back;

    draw->vtbl->SurfFillRect(draw, 0, 0, mode.width, mode.height, 0xffffff);
    screen.vtbl->SurfFillRect(&screen, 0, 0, mode.width, mode.height, 0xffffff);

    LmuxInit(&mux_bounces);
    LmuxInit(&mux_screen);

    srand(SysUpTime());
    LmuxAcquire(&mux_bounces);
    for (i = 0; i < num_bounces; i++)
    {
        bounce_t *b = bounces + i;
        b->x = rand() % (mode.width - WIDTH);
        b->y = rand() % (mode.height - HEIGHT);
        b->dx = (rand() > RAND_MAX / 2) ? 1 : -1;
        b->dy = (rand() > RAND_MAX / 2) ? 1 : -1;
        b->colour = MAKE_COLOUR(rand(), rand(), rand());
        swprintf(b->name, L"Bounce %u", i + 1);
        threads[i] = ThrCreateThread(BounceThread, b, 16, b->name);
    }
    LmuxRelease(&mux_bounces);

    WaitForKeyPress();

    quit = true;
    for (i = 0; i < num_bounces; i++)
        ThrWaitHandle(threads[i]);

    LmuxDelete(&mux_screen);
    LmuxDelete(&mux_bounces);

    back.vtbl->SurfDeleteCookie(&back);
err4:
    SurfDeleteBitmap(&back_bitmap);
err3:
    FontDelete(font);
err2:
    screen.vtbl->SurfDeleteCookie(&screen);
    if (base != NULL)
        VmmFree(base);
err1:
    VidCleanup(video, &old_mode);
err0:
    return 0;
}
