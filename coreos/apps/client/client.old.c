/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "common.h"
#include "winmgr.h"
#include "ipc.h"
#include <mgl/types.h>

extern handle_t wmgr_messages;

bool is_dragging;
MGLpoint drag_origin;
MGLrect drag_rect;

void DoMsgPaint(wnd_h wnd)
{
    static const MGLcolour colours[][2] =
    {
        //{ MGL_COLOUR(255, 128, 64),     MGL_COLOUR(159, 159, 159) },
        //{ MGL_COLOUR(255, 192, 128),    MGL_COLOUR(203, 203, 203) },
		{ 15,     7 },
        { 2,    8 },
    };

    gfx_h gfx;
    MGLrect r, pos;
    wchar_t buf[200];
    unsigned has_focus;
    int len, i;

    WndGetAttribute(wnd, WND_ATTR_POSITION, &pos, sizeof(pos));

    has_focus = WndHasFocus(wnd) ? 0 : 1;
    gfx = WndBeginPaint(wnd, NULL);
    GfxSetFillColour(gfx, colours[0][has_focus]);
    GfxFillRect(gfx, &pos);
    r = pos;
    r.bottom = r.top + 18;
    GfxSetFillColour(gfx, colours[1][has_focus]);
    GfxFillRect(gfx, &r);
    GfxSetPenColour(gfx, colours[1][has_focus]);
    GfxRectangle(gfx, &pos);

    GfxSetPenColour(gfx, 0);
    len = WndGetAttribute(wnd, WND_ATTR_TITLE, buf, sizeof(buf));
    if (len == -1)
        len = 0;
    GfxDrawText(gfx, &r, buf, len / sizeof(wchar_t));

    r = pos;
    r.left++;
    r.top += 18;
    r.right--;
    r.bottom--;
    for (i = 0; i < 50; i++)
	{
		GfxSetPenColour(gfx, rand() % 16);
        GfxLine(gfx, 
            r.left + rand() % (r.right - r.left),
            r.top + rand() % (r.bottom - r.top),
            r.left + rand() % (r.right - r.left),
            r.top + rand() % (r.bottom - r.top));
	}

    //GfxSetFillColour(gfx, MGL_COLOUR(0, 0, 128));
    //GfxFillRect(gfx, &r);

    /*GfxSetPenColour(gfx, MGL_COLOUR(255, 255, 255));
    GfxLine(gfx, (r.left + r.right) / 2 - 2, r.top - 100, (r.left + r.right) / 2 - 2, r.bottom + 100);
    GfxLine(gfx, (r.left + r.right) / 2,     r.top - 100, (r.left + r.right) / 2,     r.bottom + 100);
    GfxLine(gfx, (r.left + r.right) / 2 + 2, r.top - 100, (r.left + r.right) / 2 + 2, r.bottom + 100);

    GfxLine(gfx, r.left - 100, (r.top + r.bottom) / 2 - 2, r.right + 100, (r.top + r.bottom) / 2 - 2);
    GfxLine(gfx, r.left - 100, (r.top + r.bottom) / 2,     r.right + 100, (r.top + r.bottom) / 2    );
    GfxLine(gfx, r.left - 100, (r.top + r.bottom) / 2 + 2, r.right + 100, (r.top + r.bottom) / 2 + 2);

    GfxLine(gfx, r.left - 100, r.top - 102, r.right + 100, r.bottom +  98);
    GfxLine(gfx, r.left - 100, r.top - 100, r.right + 100, r.bottom + 100);
    GfxLine(gfx, r.left - 100, r.top -  98, r.right + 100, r.bottom + 102);

    GfxLine(gfx, r.left - 100, r.bottom +  98, r.right + 100, r.top - 102);
    GfxLine(gfx, r.left - 100, r.bottom + 100, r.right + 100, r.top - 100);
    GfxLine(gfx, r.left - 100, r.bottom + 102, r.right + 100, r.top -  98);

    GfxSetPenColour(gfx, MGL_COLOUR(255, 0, 0));
    GfxLine(gfx, (r.left + r.right) / 2, r.top - 100, (r.left + r.right) / 2, r.bottom + 100);
    GfxLine(gfx, r.left - 100, (r.top + r.bottom) / 2, r.right + 100, (r.top + r.bottom) / 2);
    GfxLine(gfx, r.left - 100, r.top - 100, r.right + 100, r.bottom + 100);
    GfxLine(gfx, r.left - 100, r.bottom + 100, r.right + 100, r.top - 100);*/

    GfxFlush();
}

void DoMsgKeyDown(wnd_h wnd, uint32_t key)
{
    wchar_t buf[200];
    int len;

    len = WndGetAttribute(wnd, WND_ATTR_TITLE, buf, sizeof(buf));
    if (len == -1)
        len = 0;
    len /= sizeof(wchar_t);
    if (key == '\b')
    {
        if (len > 0)
            len--;
    }
    else
        buf[len++] = (wchar_t) key;

    WndSetAttribute(wnd, WND_ATTR_TITLE, buf, len * sizeof(wchar_t));

    WndInvalidate(wnd, NULL);
    if (key == 27)
        WndPostQuitMessage(0);
}

void DoMsgMouseDown(wnd_h wnd, MGLpoint *pt)
{
    WndSetFocus(wnd);
    is_dragging = true;
    drag_origin = *pt;
    WndGetAttribute(wnd, WND_ATTR_POSITION, &drag_rect, sizeof(drag_rect));
    WndSetCapture(wnd, true);
}

void DoMsgMouseMove(wnd_h wnd, MGLpoint *pt)
{
    MGLpoint delta;

    if (is_dragging)
    {
        delta.x = pt->x - drag_origin.x;
        delta.y = pt->y - drag_origin.y;
        drag_rect.left += delta.x;
        drag_rect.top += delta.y;
        drag_rect.right += delta.x;
        drag_rect.bottom += delta.y;
        drag_origin = *pt;
        WndSetAttribute(wnd, WND_ATTR_POSITION, &drag_rect, sizeof(drag_rect));
    }
}

void DoMsgMouseUp(wnd_h wnd, MGLpoint *pt)
{
    is_dragging = false;
    WndSetCapture(wnd, false);
}

int main(void)
{
    uint32_t pid;
    ipc_message_t *msg;
    wnd_h wnd;
    MGLrect rc;
    wchar_t title[10];
    wnd_attr_t attribs[] =
    {
        { WND_ATTR_TITLE,       sizeof(title),  title },
        { WND_ATTR_POSITION,    sizeof(rc),     &rc },
    };

    pid = ProcGetProcessInfo()->id;
    _wdprintf(L"client %lu: connecting\n", pid);
    WndConnectIpc();
    _wdprintf(L"client %lu: connected\n", pid);

    srand(time(NULL) + pid);
    attribs[0].length = sizeof(wchar_t) * swprintf(title, L"pid %lu", pid);

    //rc.left = rand() % SCREEN_WIDTH;
    //rc.top = rand() % SCREEN_HEIGHT;
    //rc.right = 100 + rc.left + rand() % 200;
    //rc.bottom = 100 + rc.top + rand() % 200;
    rc.left = rc.top = 50;
    rc.right = rc.left + 200;
    rc.bottom = rc.top + 200;
    _wdprintf(L"client %lu: creating window\n", pid);
    wnd = WndCreate(0, attribs, _countof(attribs));
    _wdprintf(L"client %lu: created window %lu\n", pid, wnd);
    while ((msg = WndGetMessage()) &&
        msg->code != MSG_QUIT)
    {
	_wdprintf(L"client %lu: received message %lx for window %lu\n", 
	    pid, msg->code, *(wnd_h*) msg->data);

        switch (msg->code)
        {
        case MSG_PAINT:
            DoMsgPaint(*(wnd_h*) msg->data);
            break;

        case MSG_KEYDOWN:
            DoMsgKeyDown(*(wnd_h*) msg->data, *(uint32_t*) ((wnd_h*) msg->data + 1));
            break;

        case MSG_MOUSEDOWN:
            DoMsgMouseDown(*(wnd_h*) msg->data, (MGLpoint*) ((wnd_h*) msg->data + 1));
            break;

        case MSG_MOUSEMOVE:
            DoMsgMouseMove(*(wnd_h*) msg->data, (MGLpoint*) ((wnd_h*) msg->data + 1));
            break;

        case MSG_MOUSEUP:
            DoMsgMouseUp(*(wnd_h*) msg->data, (MGLpoint*) ((wnd_h*) msg->data + 1));
            break;
        }

        WndFreeMessage(msg);
    }

    _wdprintf(L"client %lu: exiting\n", pid);
    WndClose(wnd);
    WndDisconnectIpc();

    return 0;
}
