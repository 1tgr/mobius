/* $Id$ */

#include "common.h"
#include "internal.h"
#include "init.h"

#include <gui/ipc.h>
#include <mgl/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <os/keyboard.h>

point_t wmgr_cursor_pos;
static int wmgr_cursor_visible_count;
static bool wmgr_cursor_visible = false;

static lmutex_t wmgr_mux_windows;
static wnd_t **wmgr_windows;
static unsigned wmgr_num_windows = 1;
static wnd_h wmgr_focus, wmgr_capture;
static wnd_t wnd_root;
static wmgr_global_t *wmgr_global;

wnd_t *WmgrLockWindow(wnd_h hwnd)
{
    wnd_t *wnd;

    wnd = NULL;

    LmuxAcquire(&wmgr_mux_windows);
    if (hwnd >= wmgr_num_windows)
        goto error;

    if (hwnd == 0)
        wnd = &wnd_root;
    else
        wnd = wmgr_windows[hwnd];
    if (wnd == NULL)
        goto error;

    LmuxRelease(&wmgr_mux_windows);
    LmuxAcquire(&wnd->mux);
    return wnd;

error:
    LmuxRelease(&wmgr_mux_windows);
    return wnd;
}

void WmgrUnlockWindow(wnd_t *wnd)
{
    LmuxRelease(&wnd->mux);
}

bool WmgrGetPosition(wnd_t *wnd, rect_t *rect)
{
    unsigned i;

    for (i = 0; i < wnd->num_attrs; i++)
        if (wnd->attrs[i].code == WND_ATTR_POSITION)
        {
            *rect = wnd->attrs[i].data.position;
            return true;
        }

    return false;
}


void *WmgrGetClientData(wnd_t *wnd)
{
	unsigned i;

    for (i = 0; i < wnd->num_attrs; i++)
        if (wnd->attrs[i].code == WND_ATTR_CLIENT_DATA)
			return wnd->attrs[i].data.client_data;

    return NULL;
}


bool WmgrDispatchMessage(wmgr_client_t *client, wnd_message_t *msg)
{
	wnd_t *wnd;

	wnd = WmgrLockWindow(msg->wnd);
    if (wnd == NULL)
        return false;

	if (client == NULL)
        client = wnd->client;
	if (client == NULL)
	{
		WmgrUnlockWindow(wnd);
		return false;
	}

	msg->client_data = WmgrGetClientData(wnd);

	WmgrUnlockWindow(wnd);
	wnd = NULL;

    LmuxAcquire(&client->mux_messages);

    if (msg->code == MSG_MOUSEMOVE &&
        client->last_mouse_move_message != -1)
    {
        client->messages[client->last_mouse_move_message] = *msg;
        LmuxRelease(&client->mux_messages);
    }
    else
    {
        client->num_messages++;
        if (msg->code == MSG_MOUSEMOVE)
            client->last_mouse_move_message = client->num_messages - 1;

        client->messages = realloc(client->messages, sizeof(*msg) * client->num_messages);
        client->messages[client->num_messages - 1] = *msg;
        LmuxRelease(&client->mux_messages);
        EvtSignal(client->has_message, true);
    }

    return true;
}


void WmgrQueueKeyboardInput(uint32_t key)
{
	wnd_message_t msg;

	if (key & KBD_BUCKY_RELEASE)
	{
		msg.code = MSG_KEYUP;
		msg.params.key_up.key = key & ~KBD_BUCKY_RELEASE;
	}
	else
	{
		msg.code = MSG_KEYDOWN;
		msg.params.key_down.key = key;
	}

	msg.wnd = wmgr_focus;
    WmgrDispatchMessage(NULL, &msg);
}


void WmgrQueueMouseInput(unsigned event, int x, int y)
{
    wnd_message_t msg;

    LmuxAcquire(&wmgr_mux_windows);
    if (wmgr_capture != 0)
	{
        msg.wnd = wmgr_capture;
		LmuxRelease(&wmgr_mux_windows);
	}
    else
	{
		LmuxRelease(&wmgr_mux_windows);
        msg.wnd = WmgrWindowAtPoint(0, x, y);
	}

	msg.code = event;
	msg.params.mouse_move.x = x;
	msg.params.mouse_move.y = y;
    WmgrDispatchMessage(NULL, &msg);
}


wnd_h WmgrCreate(wnd_h parent, const wnd_attr_t *attrs, unsigned num_attrs)
{
    unsigned i;
    wnd_t *wnd, *wnd_parent;
    wnd_h ret;

    wnd_parent = WmgrLockWindow(parent);
    if (wnd_parent == NULL)
	{
		_wdprintf(L"WmgrCreate: invalid parent window: %u\n", parent);
        return 0;
	}

    LmuxAcquire(&wmgr_mux_windows);
    wnd = malloc(sizeof(*wnd));
    if (wnd == NULL)
    {
        LmuxRelease(&wmgr_mux_windows);
		WmgrUnlockWindow(wnd_parent);
        return 0;
    }

    memset(wnd, 0, sizeof(*wnd));

    wmgr_windows = realloc(wmgr_windows, sizeof(wnd_t*) * (wmgr_num_windows + 1));
    if (wmgr_windows == NULL)
    {
        free(wnd);
        LmuxRelease(&wmgr_mux_windows);
		WmgrUnlockWindow(wnd_parent);
        return 0;
    }

    ret = wmgr_num_windows++;
    wmgr_windows[ret] = wnd;
    LmuxRelease(&wmgr_mux_windows);

    LmuxInit(&wnd->mux);
    wnd->handle = ret;
    wnd->client = WmgrGetClient();
    wnd->num_attrs = num_attrs;
    wnd->parent = parent;
    wnd->attrs = malloc(num_attrs * sizeof(*wnd->attrs));
    GmgrInitDefaultGraphics(&wnd->gfx);
    LIST_ADD(wnd_parent->child, wnd);
    WmgrUnlockWindow(wnd_parent);

    if (wnd->attrs == NULL)
        return 0;

    //printf("WmgrCreate: parent = %lu, num_attrs = %u\n",
        //parent, num_attrs);
    for (i = 0; i < num_attrs; i++)
    {
        //printf("\tattr %u: code = %lx, length = %lu ", 
            //i, attrs[i].code, (unsigned long) attrs[i].length);
        wnd->attrs[i] = attrs[i];

        /*switch (attrs[i].code)
        {
        case WND_ATTR_TITLE:
            printf("title: %S\n", (wchar_t*) attrs[i].data);
            break;

        case WND_ATTR_POSITION:
            {
                rect_t *rc;
                rc = (rect_t*) attrs[i].data;
                printf("pos: (%d,%d)-(%d,%d)\n", rc->left, rc->top, rc->right, rc->bottom);
                break;
            }

        case WND_ATTR_CLIENTDATA:
            printf("data: %lx\n", *(uint32_t*) attrs[i].data);
            break;

        default:
            printf("(unknown)\n");
            break;
        }*/
    }

    WmgrInvalidate(ret, NULL);
    WmgrSetFocus(ret);
    return ret;
}


void WmgrClose(wnd_h hwnd)
{
    wnd_t *wnd, *child, *parent;
    rect_t pos;

    wnd = WmgrLockWindow(hwnd);
    if (wnd == NULL)
        return;

	while ((child = wnd->child_first) != NULL)
	{
		WmgrUnlockWindow(wnd);
		WmgrClose(child->handle);
		wnd = WmgrLockWindow(hwnd);
		assert(wnd != NULL);
	}

    LmuxAcquire(&wmgr_mux_windows);
    if (hwnd == wmgr_focus)
        wmgr_focus = wnd->parent;
	LmuxRelease(&wmgr_mux_windows);

    if (wnd != &wnd_root)
    {
        parent = WmgrLockWindow(wnd->parent);
        if (parent != NULL)
        {
            LIST_REMOVE(parent->child, wnd);
            WmgrUnlockWindow(parent);
        }
    }

    if (WmgrGetPosition(wnd, &pos))
        WmgrInvalidate(0, &pos);

    free(wnd->attrs);
    free(wnd->gfx.clip.rects);

	LmuxAcquire(&wmgr_mux_windows);
    if (wnd == &wnd_root)
        LmuxRelease(&wnd->mux);
    else
    {
        free(wnd);
        wmgr_windows[hwnd] = NULL;
    }
	LmuxRelease(&wmgr_mux_windows);
}


void WmgrPostQuitMessage(int code)
{
	wnd_message_t msg;
	msg.code = MSG_QUIT;
	msg.wnd = 0;
	msg.params.quit.code = code;
    WmgrDispatchMessage(WmgrGetClient(), &msg);
}


gfx_h WmgrBeginPaint(wnd_h hwnd, rect_t *rect)
{
    wnd_t *wnd;

    wnd = WmgrLockWindow(hwnd);
    if (wnd == NULL)
        return 0;

    WmgrUpdateClip(&wnd->gfx, wnd);
    *rect = wnd->invalid;
    wnd->invalid.left = wnd->invalid.top = 
        wnd->invalid.right = wnd->invalid.bottom = 0;
    wnd->is_painting = false;
    WmgrUnlockWindow(wnd);

    return hwnd | 0x80000000;
}


void WmgrInvalidate(wnd_h hwnd, const rect_t *rect)
{
    wnd_t *wnd, *child, *next;
    rect_t pos, r;

    wnd = WmgrLockWindow(hwnd);
    if (wnd == NULL)
        return;

    if (!WmgrGetPosition(wnd, &pos))
        pos.left = pos.top = pos.right = pos.bottom = 0;
    if (rect == NULL)
        rect = &pos;

    /*RectUnion(&r, &wnd->invalid, rect);
    r.left = max(r.left, pos.left);
    r.top = max(r.top, pos.top);
    r.left = min(r.left, pos.right);
    r.top = min(r.top, pos.bottom);
    r.right = min(r.right, pos.right);
    r.bottom = min(r.bottom, pos.right);
    r.right = max(r.right, pos.left);
    r.bottom = max(r.bottom, pos.top);*/
    /*r = *rect;
    r.left = max(r.left, pos.left);
    r.top = max(r.top, pos.top);
    r.left = min(r.left, pos.right);
    r.top = min(r.top, pos.bottom);
    r.right = min(r.right, pos.right);
    r.bottom = min(r.bottom, pos.bottom);
    r.right = max(r.right, pos.left);
    r.bottom = max(r.bottom, pos.top);*/
	RectUnion(&r, &wnd->invalid, rect);
    if (!RectIsEmpty(&r))
    {
        wnd->invalid = r;
        for (child = wnd->child_first; child != NULL; child = next)
        {
            next = child->next;
            WmgrUnlockWindow(wnd);
            WmgrInvalidate(child->handle, &r);
            wnd = WmgrLockWindow(hwnd);
            assert(wnd != NULL);
        }

        if (!wnd->is_painting)
        {
			wnd_message_t msg;

			WmgrUnlockWindow(wnd);
			msg.code = MSG_PAINT;
			msg.wnd = hwnd;
            WmgrDispatchMessage(NULL, &msg);

			wnd = WmgrLockWindow(hwnd);
			assert(wnd != NULL);
            wnd->is_painting = true;
        }

		WmgrUnlockWindow(wnd);
    }
    else
        WmgrUnlockWindow(wnd);
}


bool WmgrGetAttribute(wnd_h hwnd, uint32_t code, wnd_data_t *data)
{
    wnd_t *wnd;
    unsigned i;

    wnd = WmgrLockWindow(hwnd);
    if (wnd == NULL)
        return false;

    for (i = 0; i < wnd->num_attrs; i++)
        if (wnd->attrs[i].code == code)
        {
			*data = wnd->attrs[i].data;
			WmgrUnlockWindow(wnd);
			return true;
        }

    WmgrUnlockWindow(wnd);
    return false;
}


void WmgrSetAttribute(wnd_h hwnd, uint32_t code, const wnd_data_t *data)
{
    wnd_t *wnd;
    unsigned i;
    wnd_attr_t *a;
    rect_t old_pos;

    wnd = WmgrLockWindow(hwnd);
    if (wnd == NULL)
        return;

    a = NULL;
    for (i = 0; i < wnd->num_attrs; i++)
        if (wnd->attrs[i].code == code)
        {
            a = wnd->attrs + i;
            break;
        }

    if (a == NULL)
    {
        wnd->attrs = realloc(wnd->attrs, sizeof(wnd_attr_t) * (wnd->num_attrs + 1));
        a = wnd->attrs + (wnd->num_attrs++);
        a->code = code;
    }

    if (code == WND_ATTR_POSITION)
		old_pos = a->data.position;

    a->data = *data;

    WmgrUnlockWindow(wnd);

    if (code == WND_ATTR_POSITION)
    {
        WmgrInvalidate(0, &old_pos);
        WmgrInvalidate(hwnd, &a->data.position);
    }
}


bool WmgrHasFocus(wnd_h hwnd)
{
    return wmgr_focus == hwnd;
}


void WmgrSetFocus(wnd_h hwnd)
{
    wnd_h old;
    wnd_t *wnd, *parent;

    wnd = WmgrLockWindow(hwnd);
    if (wnd == NULL)
        return;
    parent = WmgrLockWindow(wnd->parent);
    if (parent != NULL)
    {
        LIST_REMOVE(parent->child, wnd);
        LIST_ADD(parent->child, wnd);
        WmgrUnlockWindow(parent);
    }
    WmgrUnlockWindow(wnd);

    LmuxAcquire(&wmgr_mux_windows);
    old = wmgr_focus;
	LmuxRelease(&wmgr_mux_windows);

    if (hwnd != old)
    {
        wmgr_focus = hwnd;
        WmgrInvalidate(old, NULL);
        WmgrInvalidate(wmgr_focus, NULL);
    }
}


wnd_h WmgrWindowAtPoint(wnd_h root, int x, int y)
{
    wnd_t *parent, *child;
    rect_t pos;
    wnd_h ret;

    parent = WmgrLockWindow(root);
    if (parent == NULL)
        return 0;

    ret = 0;
    for (child = parent->child_last; child != NULL; child = child->prev)
    {
        ret = WmgrWindowAtPoint(child->handle, x, y);
        if (ret != 0)
            break;
    }

    if (ret == 0)
    {
        if (WmgrGetPosition(parent, &pos) &&
            x >= pos.left &&
            y >= pos.top &&
            x < pos.right &&
            y < pos.bottom)
            ret = root;
    }

    WmgrUnlockWindow(parent);
    return ret;
}


wnd_h WmgrOwnRoot(const wnd_attr_t *attrs, unsigned num_attrs)
{
    wnd_t *wnd;
    unsigned i;
    rect_t rect;

    wnd = WmgrLockWindow(0);
    if (wnd == NULL)
        return -1;

    wnd->client = WmgrGetClient();
    wnd->is_painting = false;
    rect = wnd->invalid;
    WmgrUnlockWindow(wnd);

    /*_wdprintf(L"WmgrOwnRoot:\n");*/
    for (i = 0; i < num_attrs; i++)
    {
		WmgrSetAttribute(0, attrs[i].code, &attrs[i].data);

        /*_wdprintf(L"\tattr %u: code = %lx ", i, attrs[i].code);

        switch (attrs[i].code)
        {
        case WND_ATTR_POSITION:
            _wdprintf(L"pos: (%d,%d)-(%d,%d)\n", 
				attrs[i].data.position.left,
				attrs[i].data.position.top,
				attrs[i].data.position.right,
				attrs[i].data.position.bottom);
            break;

        case WND_ATTR_CLIENT_DATA:
            _wdprintf(L"data: %p\n", 
				attrs[i].data.client_data);
            break;

        default:
            _wdprintf(L"(unknown)\n");
            break;
        }*/
    }

	WmgrInvalidate(0, &rect);
    return 0;
}


void WmgrSetCapture(wnd_h hwnd, bool set)
{
    LmuxAcquire(&wmgr_mux_windows);

    if (set)
        wmgr_capture = hwnd;
    else
        wmgr_capture = 0;

    LmuxRelease(&wmgr_mux_windows);
}


void WmgrGetMessage(void)
{
    wmgr_client_t *client;
	wnd_params_t params_out;
    unsigned i;

    client = WmgrGetClient();
    ThrWaitHandle(client->has_message);

    LmuxAcquire(&client->mux_messages);
    params_out.wnd_get_message_reply.msg = client->messages[0];
    client->num_messages--;
    for (i = 0; i < client->num_messages; i++)
        client->messages[i] = client->messages[i + 1];
    if (client->last_mouse_move_message > -1)
    {
        if (client->last_mouse_move_message == 0)
            assert(params_out.wnd_get_message_reply.msg.code == MSG_MOUSEMOVE);
        client->last_mouse_move_message--;
    }
    LmuxRelease(&client->mux_messages);

    IpcCall(client->ipc, SYS_WndGetMessage, &params_out);
}


void WmgrPostMessage(const wnd_message_t *msg)
{
	wnd_t *wnd;
	wmgr_client_t *client;
	wnd_message_t temp;

	temp = *msg;
	wnd = WmgrLockWindow(temp.wnd);
	if (wnd == NULL)
		return;

	client = wnd->client;
	WmgrUnlockWindow(wnd);

	WmgrDispatchMessage(client, &temp);
}


void WmgrSetCursorPos(const point_t *pt)
{
	//LmuxAcquire(&gmgr_draw);
	if (wmgr_cursor_visible)
		GmgrMoveCursor(wmgr_cursor_pos.x, wmgr_cursor_pos.y, pt->x, pt->y);
	wmgr_cursor_pos = *pt;
	//LmuxRelease(&gmgr_draw);
}


void WmgrShowCursor(bool show)
{
	//LmuxAcquire(&gmgr_draw);

	if (show)
		wmgr_cursor_visible_count++;
	else
		wmgr_cursor_visible_count--;

	if (wmgr_cursor_visible != (wmgr_cursor_visible_count > 0))
	{
		GmgrDrawCursor(wmgr_cursor_pos.x, wmgr_cursor_pos.y, show);
		wmgr_cursor_visible = wmgr_cursor_visible_count > 0;
	}

	//LmuxRelease(&gmgr_draw);
}


bool WmgrInit(void)
{
	wnd_data_t data;

	wmgr_global = VmmAlloc(PAGE_ALIGN_UP(sizeof(*wmgr_global)) / PAGE_SIZE,
		0,
		VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
	if (wmgr_global == NULL)
		return false;

	VmmShare(wmgr_global, WMGR_GLOBAL_NAME);
	wmgr_global->video_mode = gmgr_screen->mode;

    LmuxInit(&wmgr_mux_windows);
    LmuxInit(&wnd_root.mux);
    GmgrInitDefaultGraphics(&wnd_root.gfx);

	data.position.left = data.position.top = 0;
	data.position.right = gmgr_screen->mode.width;
	data.position.bottom = gmgr_screen->mode.height;
    WmgrSetAttribute(0, WND_ATTR_POSITION, &data);

	wmgr_cursor_pos.x = (data.position.left + data.position.right) / 2;
	wmgr_cursor_pos.y = (data.position.top + data.position.bottom) / 2;
    return true;
}
