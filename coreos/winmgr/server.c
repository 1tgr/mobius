/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

#include "common.h"
#include "internal.h"
#include "init.h"
#include <gui/ipc.h>

#include <os/keyboard.h>
#include <os/mouse.h>

lmutex_t mux_clients;
wmgr_client_t *client_first, *client_last;
bool wmgr_wantquit;

#ifdef __MOBIUS__
static int WmgrKeyboardThread(void *param)
{
    uint32_t ch;
    handle_t keyboard;

    keyboard = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
	while (FsRead(keyboard, &ch, 0, sizeof(ch), NULL))
	{
		if (ch == (KBD_BUCKY_CTRL | KBD_BUCKY_ALT | KEY_DEL))
            SysShutdown(SHUTDOWN_REBOOT);
		else
			WmgrQueueKeyboardInput(ch);
	}

	return errno;
}

static int WmgrMouseThread(void *param)
{
	mouse_packet_t pkt;
	handle_t mouse;
	point_t pt;
	uint32_t old_buttons;

	old_buttons = 0;
	mouse = (handle_t) param;
	WmgrShowCursor(true);

	while (FsRead(mouse, &pkt, 0, sizeof(pkt), NULL))
	{
		pt = wmgr_cursor_pos;
		pt.x += pkt.dx;
		pt.y += pkt.dy;

		if (pt.x < 0)
			pt.x = 0;
		if (pt.x >= gmgr_screen->mode.width)
			pt.x = gmgr_screen->mode.width - 1;

		if (pt.y < 0)
			pt.y = 0;
		if (pt.y >= gmgr_screen->mode.height)
			pt.y = gmgr_screen->mode.height - 1;

		WmgrSetCursorPos(&pt);

		if (pkt.dx != 0 || pkt.dy != 0)
			WmgrQueueMouseInput(MSG_MOUSEMOVE, pt.x, pt.y);

		if (old_buttons == 0 && 
			pkt.buttons != 0)
			WmgrQueueMouseInput(MSG_MOUSEDOWN, pt.x, pt.y);
		else if (old_buttons != 0 &&
			pkt.buttons == 0)
			WmgrQueueMouseInput(MSG_MOUSEUP, pt.x, pt.y);

		old_buttons = pkt.buttons;
	}

	HndClose(mouse);
	WmgrShowCursor(false);
	return errno;
}
#endif


static void WmgrSrvConnect(wmgr_client_t *client, const wnd_params_t *params, 
						   void *extra, size_t extra_length)
{
	wnd_params_t params_out;
	handle_t c;

	swprintf(params_out.wnd_connect_reply.shared_area, L"wmgr_%08x", client->ipc);
	client->shared = VmmAlloc(PAGE_ALIGN_UP(sizeof(*client->shared)) / PAGE_SIZE,
		0,
		VM_MEM_READ | VM_MEM_WRITE | VM_MEM_USER);
	c = VmmShare(client->shared, params_out.wnd_connect_reply.shared_area);
	HndClose(c);

	/*_wdprintf(L"WmgrSrvConnect: name = %s, shared = %p\n", 
		params_out.wnd_connect_reply.shared_area, 
		client->shared);*/

	swprintf(client->shared->message_event_name, L"wmgr_msg_%08x", client->has_message);
	HndExport(client->has_message, client->shared->message_event_name);

	IpcCall(client->ipc, SYS_WndConnect, &params_out);
}


static void WmgrSrvCreate(wmgr_client_t *client, const wnd_params_t *params, 
						  void *extra, size_t extra_length)
{
    wnd_params_t params_out;

    params_out.wnd_create_reply.wnd = WmgrCreate(params->wnd_create.parent, 
		extra, 
		extra_length / sizeof(wnd_attr_t));

	IpcCall(client->ipc, SYS_WndCreate, &params_out);
}


static void WmgrSrvBeginPaint(wmgr_client_t *client, const wnd_params_t *params, 
							  void *extra, size_t extra_length)
{
	wnd_params_t params_out;

    params_out.wnd_begin_paint_reply.gfx = WmgrBeginPaint(
		params->wnd_begin_paint.wnd, 
		&params_out.wnd_begin_paint_reply.rect);

    IpcCall(client->ipc, SYS_WndBeginPaint, &params_out);
}


static void WmgrSrvGetAttribute(wmgr_client_t *client, const wnd_params_t *params, 
								void *extra, size_t extra_length)
{
	wnd_params_t params_out;

    if (WmgrGetAttribute(params->wnd_get_attribute.wnd, 
		params->wnd_get_attribute.code, 
		&params_out.wnd_get_attribute_reply.attr.data))
		params_out.wnd_get_attribute_reply.attr.code = params->wnd_get_attribute.code;
	else
		params_out.wnd_get_attribute_reply.attr.code = 0;

	IpcCall(client->ipc, SYS_WndGetAttribute, &params_out);
}


static void WmgrSrvHasFocus(wmgr_client_t *client, const wnd_params_t *params, 
							void *extra, size_t extra_length)
{
	wnd_params_t params_out;

    params_out.wnd_has_focus_reply.has_focus = WmgrHasFocus(params->wnd_has_focus.wnd);

	IpcCall(client->ipc, SYS_WndHasFocus, &params_out);
}


static void WmgrSrvWindowAtPoint(wmgr_client_t *client, const wnd_params_t *params, 
								 void *extra, size_t extra_length)
{
	wnd_params_t params_out;

    params_out.wnd_window_at_point_reply.wnd = 
		WmgrWindowAtPoint(params->wnd_window_at_point.root, 
			params->wnd_window_at_point.x, 
			params->wnd_window_at_point.y);

    IpcCall(client->ipc, SYS_WndWindowAtPoint, &params_out);
}


static void WmgrSrvOwnRoot(wmgr_client_t *client, const wnd_params_t *params, 
						   void *extra, size_t extra_length)
{
    wnd_params_t params_out;

    params_out.wnd_own_root_reply.wnd = 
		WmgrOwnRoot(extra, 
			extra_length / sizeof(wnd_attr_t));

    IpcCall(client->ipc, SYS_WndOwnRoot, &params_out);
}


static void WmgrSrvAttachInputDevice(wmgr_client_t *client, const wnd_params_t *params, 
									 void *extra, size_t extra_length)
{
	wchar_t *name;
	wnd_params_t params_out;
	handle_t mouse, c;

	name = malloc(extra_length + sizeof(wchar_t));
	memcpy(name, extra, extra_length);
	name[extra_length / sizeof(wchar_t)] = '\0';

	mouse = FsOpen(name, FILE_READ);
	free(name);

	if (mouse == NULL)
		params_out.wnd_attach_input_device_reply.error = errno;
	else
	{
		c = ThrCreateThread(WmgrMouseThread, (void*) mouse, 16, L"WmgrMouseThread");
		HndClose(c);
		params_out.wnd_attach_input_device_reply.error = 0;
	}

	IpcCall(client->ipc, SYS_WndAttachInputDevice, &params_out);
}


static void GmgrSrvCreateBitmap(wmgr_client_t *client, const wnd_params_t *params, 
								void *extra, size_t extra_length)
{
	wnd_params_t params_out;

	params_out.gfx_create_bitmap_reply.gfx = 
		GmgrCreateBitmap(params->gfx_create_bitmap.width, 
			params->gfx_create_bitmap.height, 
			&params_out.gfx_create_bitmap_reply.desc);

	IpcCall(client->ipc, SYS_GfxCreateBitmap, &params_out);
}


static void GmgrSrvGetTextSize(wmgr_client_t *client, const wnd_params_t *params, 
							   void *extra, size_t extra_length)
{
	wnd_params_t params_out;

	GmgrGetTextSize(params->gfx_get_text_size.gfx,
		extra,
		extra_length / sizeof(wchar_t),
		&params_out.gfx_get_text_size_reply.size);

	IpcCall(client->ipc, SYS_GfxGetTextSize, &params_out);
}


wmgr_client_t *WmgrGetClient(void)
{
    return ThrGetThreadInfo()->param;
}


handle_t IpcGetDefault(void)
{
	return 0;
}


void WmgrWin32ServerIsExiting(void);

static int WmgrServerThread(void *param)
{
    wmgr_client_t *client;
	wnd_params_t params;
    void *extra;
    size_t extra_length;
    bool disconnected;
	uint32_t code;

	ThrGetThreadInfo()->exception_handler = NULL;
    client = WmgrGetClient();

    LmuxAcquire(&mux_clients);
    LIST_ADD(client, client);
    LmuxRelease(&mux_clients);

    disconnected = false;
    while (!disconnected &&
        (code = IpcReceiveExtra(client->ipc, &params, &extra, &extra_length)) != 0)
    {
        //client->msg = &params;

		switch (code)
        {
        case SYS_WndConnect:
			WmgrSrvConnect(client, &params, extra, extra_length);
            break;

        case SYS_WndDisconnect:
			/*_wdprintf(L"winmgr: got disconnect message\n");*/
            disconnected = true;
            break;

        case SYS_WndCreate:
            WmgrSrvCreate(client, &params, extra, extra_length);
            break;

        case SYS_WndClose:
            WmgrClose(params.wnd_close.wnd);
            break;

        case SYS_WndPostQuitMessage:
            WmgrPostQuitMessage(params.wnd_post_quit_message.code);
            break;

        case SYS_WndBeginPaint:
            WmgrSrvBeginPaint(client, &params, extra, extra_length);
            break;

        case SYS_WndInvalidate:
			if (params.wnd_invalidate.rect.left == 0 &&
				params.wnd_invalidate.rect.top == 0 &&
				params.wnd_invalidate.rect.right == 0 &&
				params.wnd_invalidate.rect.bottom == 0)
				WmgrInvalidate(params.wnd_invalidate.wnd, NULL);
			else
				WmgrInvalidate(params.wnd_invalidate.wnd, &params.wnd_invalidate.rect);
			break;

        case SYS_WndGetAttribute:
            WmgrSrvGetAttribute(client, &params, extra, extra_length);
            break;

        case SYS_WndSetAttribute:
            WmgrSetAttribute(params.wnd_set_attribute.wnd,
				params.wnd_set_attribute.attr.code,
				&params.wnd_set_attribute.attr.data);
            break;

        case SYS_WndHasFocus:
            WmgrSrvHasFocus(client, &params, extra, extra_length);
            break;

        case SYS_WndSetFocus:
            WmgrSetFocus(params.wnd_set_focus.wnd);
            break;

        case SYS_WndWindowAtPoint:
            WmgrSrvWindowAtPoint(client, &params, extra, extra_length);
            break;

        case SYS_WndOwnRoot:
            WmgrSrvOwnRoot(client, &params, extra, extra_length);
            break;

        case SYS_WndSetCapture:
			WmgrSetCapture(params.wnd_set_capture.wnd, params.wnd_set_capture.set);
            break;

        case SYS_WndGetMessage:
            WmgrGetMessage();
            break;

		case SYS_WndPostMessage:
			WmgrPostMessage(&params.wnd_post_message.msg);
			break;

		case SYS_WndAttachInputDevice:
			WmgrSrvAttachInputDevice(client, &params, extra, extra_length);
			break;

        case SYS_GfxFlush:
            GmgrFlush(extra, extra_length);
            break;

		case SYS_GfxCreateBitmap:
			GmgrSrvCreateBitmap(client, &params, extra, extra_length);
			break;

		case SYS_GfxClose:
			GmgrClose(params.gfx_close.gfx);
			break;

		case SYS_GfxGetTextSize:
			GmgrSrvGetTextSize(client, &params, extra, extra_length);
			break;
        }

        //client->msg = NULL;
        free(extra);
    }

    LmuxAcquire(&mux_clients);
    LIST_REMOVE(client, client);
    LmuxRelease(&mux_clients);

#ifdef __MOBIUS__
    HndClose(client->ipc);
#else
	CloseHandle(client->ipc);
#endif
    free(client->messages);

	if (client->shared != NULL)
	{
		HndExport(client->has_message, NULL);
		VmmFree(client->shared);
	}

    _wdprintf(L"WmgrServerThread: client exited, %u messages remaining\n", client->num_messages);
    free(client);

    if (wmgr_wantquit &&
        client_last == NULL)
    {
#ifdef WIN32
        WmgrWin32ServerIsExiting();
#endif
		_wdprintf(L"winmgr: last client exiting\n");
        exit(0);
    }
	else
#ifdef __MOBIUS__
		ThrExitThread(0);
#else
		ExitThread(0);
#endif
}

#if defined(__MOBIUS__)

int main(void)
{
    static const wchar_t server_name[] = SYS_PORTS L"/winmgr";
    handle_t server, c;
    wmgr_client_t *client;
	int en = errno;

	ThrGetThreadInfo()->exception_handler = NULL;

	if (!GmgrInit() ||
		!WmgrInit())
        return 1;

    LmuxInit(&mux_clients);
    server = FsCreate(server_name, 0);
	if (server == NULL)
	{
		_wdprintf(L"winmgr: FsCreate(%s) failed: %s\n",
			server_name, _wcserror(errno));
		return 1;
	}

    c = ThrCreateThread(WmgrKeyboardThread, 0, 16, L"WmgrKeyboardThread");
	HndClose(c);

	/*_wdprintf(L"The Mobius window manager!\nServer port is %lu\n", server);*/

    c = ProcSpawnProcess(L"/mobius/desktop.exe", ProcGetProcessInfo());
    HndClose(c);

    while ((c = PortAccept(server, FILE_READ | FILE_WRITE)) != NULL)
    {
		client = malloc(sizeof(*client));
        if (client == NULL)
            continue;

        client->ipc = c;
        //client->msg = NULL;
        client->num_messages = 0;
        client->messages = NULL;
        client->has_message = EvtCreate(false);
        client->last_mouse_move_message = -1;
        LmuxInit(&client->mux_messages);
        ThrCreateThread(WmgrServerThread, client, 16, L"WmgrServerThread");
    }

	en = errno;
	_wdprintf(L"winmgr: exiting\n");
    HndClose(server);
    return en;
}

#elif defined(WIN32)
void WmgrWin32Init(void);
handle_t ThrCreateThread(void (*entry)(void), void *param, unsigned priority);

int main(void)
{
    HANDLE server;
    unsigned clients_started;
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };

    WmgrInit();
    WmgrWin32Init();

    si.cb = sizeof(si);
    si.dwFlags = 0;
    clients_started = 0;
    LmuxInit(&mux_clients);
    while (true)
    {
        server = CreateNamedPipe(_T("\\\\.\\pipe\\WmgrServer"), PIPE_ACCESS_DUPLEX, 0, 
            PIPE_UNLIMITED_INSTANCES, BUFSIZ * 16, BUFSIZ * 16, 0, NULL);

        if (clients_started == 0)
        {
            CreateProcess(NULL, "desktop\\debug\\desktop.exe", NULL, NULL, 
                FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
            clients_started++;
        }
        else if (clients_started < 2)
        {
            if (!CreateProcess(NULL, "opclient\\debug\\opclient.exe", NULL, NULL, 
                FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
                break;
            clients_started++;
        }

        if (ConnectNamedPipe(server, NULL) ||
            GetLastError() == ERROR_PIPE_CONNECTED)
        {
            wmgr_client_t *client;
            int h;

            //h = _open_osfhandle((long) server, 0);
            client = malloc(sizeof(*client));
            if (client == NULL)
            {
                CloseHandle(server);
                continue;
            }

            //client->ipc = _fdopen(h, "r+b");
			client->ipc = server;
            client->msg = NULL;
            client->num_messages = 0;
            client->messages = NULL;
            client->has_message = EvtAlloc();
            client->last_mouse_move_message = -1;
            LmuxInit(&client->mux_messages);
            ThrCreateThread(WmgrServerThread, client, 16);
        }
        else
            CloseHandle(server);
    }

    return 0;
}

#endif
