/*
 * $History: gfxbase.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:46
 * Updated in $/coreos/winmgr/gfx
 * Don't force 8bpp graphics mode
 * Added history block
 */

#include "common.h"
#include "internal.h"
#include "init.h"

#include <wchar.h>
#include <string.h>
#include <errno.h>

extern lmutex_t gmgr_draw;
extern lmutex_t gmgr_mux_gfxs;

gmgr_surface_t *gmgr_screen;
font_t *gmgr_font;

void GmgrToScreen(rect_t *dest, const rect_t *mgl)
{
    dest->left = mgl->left;
    dest->top = mgl->top;
    dest->right = mgl->right;
    dest->bottom = mgl->bottom;
}


void GmgrToScreenPoint(int *ax, int *ay, int x, int y)
{
    *ax = x;
    *ay = y;
}


void GmgrToVirtual(rect_t *dest, const rect_t *virt)
{
    dest->left = virt->left;
    dest->top = virt->top;
    dest->right = virt->right;
    dest->bottom = virt->bottom;
}


void GmgrToVirtualPoint(int *ax, int *ay, int x, int y)
{
    *ax = x;
    *ay = y;
}


static void GmgrCleanup(void)
{
    FontDelete(gmgr_font);
    gmgr_font = NULL;
    LmuxDelete(&gmgr_draw);
    LmuxDelete(&gmgr_mux_gfxs);
    GmgrCloseSurface(gmgr_screen);
    gmgr_screen = NULL;
}


bool GmgrInit(void)
{
    params_vid_t params;
    handle_t device;

    LmuxInit(&gmgr_draw);
    LmuxInit(&gmgr_mux_gfxs);
    atexit(GmgrCleanup);

    device = FsOpen(SYS_DEVICES L"/Classes/video0", FILE_READ | FILE_WRITE);
    if (device == NULL)
    {
        _wdprintf(SYS_DEVICES L"/Classes/video0" L": %s\n", _wcserror(errno));
        goto error0;
    }

    memset(&params.vid_setmode, 0, sizeof(params.vid_setmode));
    if (!FsRequestSync(device, VID_SETMODE, &params, sizeof(params), NULL))
    {
        _wdprintf(L"VID_SETMODE: %s\n", _wcserror(errno));
        goto error1;
    }

    gmgr_screen = GmgrCreateDeviceSurface(device, &params.vid_setmode);
    if (gmgr_screen == NULL)
    {
        _wdprintf(L"GmgrCreateDeviceSurface: %s\n", _wcserror(errno));
        goto error1;
    }

    gmgr_font = FontLoad(L"/Mobius/veramono.ttf", 12 * 64, 
        gmgr_screen->mode.bitsPerPixel < 8 ? FB_FONT_MONO : FB_FONT_SMOOTH);
    if (gmgr_font == NULL)
    {
        _wdprintf(L"/Mobius/veramono.ttf: %s\n", _wcserror(errno));
        goto error2;
    }

    if (!GmgrInitCursor())
        goto error3;

    return true;

error3:
    FontDelete(gmgr_font);
    gmgr_font = NULL;
error2:
    GmgrCloseSurface(gmgr_screen);
    gmgr_screen = NULL;
error1:
    HndClose(device);
error0:
    return false;
}
