/* $Id$ */

#include <gui/winmgr.h>
#include <gui/ipc.h>

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>

#include <os/rtl.h>
#include <os/defs.h>
#include <os/syscall.h>

#include "internal.h"

wnd_thread_t __declspec(dllexport) *WndGetThread(void)
{
    return ThrGetThreadInfo()->wmgr_info;
}


void __declspec(dllexport) WndGetScreenSize(point_t *pt)
{
	pt->x = wmgr_global->video_mode.width;
	pt->y = wmgr_global->video_mode.height;
}


handle_t IpcGetDefault(void)
{
	return WndGetThread()->ipc;
}


/**
 * Connects to the window manager
 *
 * \return true if the connection succeeded, false otherwise
 */
bool __declspec(dllexport) WndConnectIpc(void)
{
    wnd_thread_t *thread;
	thread_info_t *thread_info;
	wnd_params_t params;

	thread_info = ThrGetThreadInfo();
	if (thread_info->wmgr_info != NULL)
	{
		_wdprintf(L"%s: WndConnectIpc: already connected\n",
			ProcGetProcessInfo()->module_first->name);
		return true;
	}

    thread = malloc(sizeof(*thread));
    if (thread == NULL)
        return false;

	thread->ipc = FsOpen(SYS_PORTS L"/winmgr", FILE_READ | FILE_WRITE);
	if (thread->ipc == NULL)
	{
		_wdprintf(L"%s: FsOpen(" SYS_PORTS L"/winmgr" L") failed: %s\n",
			ProcGetProcessInfo()->module_first->name,
			_wcserror(errno));
		free(thread);
		return false;
	}

	thread_info->wmgr_info = thread;

    if (!IpcCall(NULL, SYS_WndConnect, NULL) ||
		!IpcReceive(NULL, &params))
	{
		free(thread);
		thread_info->wmgr_info = NULL;
		return false;
	}

	thread->shared_handle = HndOpen(params.wnd_connect_reply.shared_area);
	if (thread->shared_handle == NULL)
	{
		thread->shared = NULL;
		thread->got_message = NULL;
	}
	else
	{
		thread->shared = VmmMapSharedArea(thread->shared_handle, 0, VM_MEM_READ | VM_MEM_USER);
		if (thread->shared == NULL)
			thread->got_message = NULL;
		else
			thread->got_message = HndOpen(thread->shared->message_event_name);
	}

	//_wdprintf(L"%s/%u: ipc = %u, shared = %p\n",
		//ProcGetProcessInfo()->module_first->name, ThrGetThreadInfo()->id,
        //thread->ipc, thread->shared);

    return true;
}


/**
 * Disconnects from the window manager
 */
void __declspec(dllexport) WndDisconnectIpc(void)
{
    wnd_thread_t *thread;

    thread = WndGetThread();
    if (thread != NULL)
    {
        IpcCall(NULL, SYS_WndDisconnect, NULL);
        HndClose(thread->ipc);
        HndClose(thread->got_message);
        HndClose(thread->shared_handle);
        VmmFree(thread->shared);
        free(thread);
        ThrGetThreadInfo()->wmgr_info = NULL;
    }
}


handle_t __declspec(dllexport) WndGetMessageEvent(void)
{
	return WndGetThread()->got_message;
}


/**
 * Retrieves the oldest message from the calling thread's message queue
 *
 * If there are no messages in the queue, this function waits until one arrives.
 *
 * \return A pointer to an ipc_message_t structure. Call WndFreeMessage to free.
 */
bool __declspec(dllexport) WndGetMessage(wnd_message_t *msg)
{
	wnd_params_t params;

    if (!IpcCall(NULL, SYS_WndGetMessage, NULL) ||
		!IpcReceive(NULL, &params))
		return false;

	*msg = params.wnd_get_message_reply.msg;
	return true;
}


void __declspec(dllexport) WndPostMessage(const wnd_message_t *msg)
{
	wnd_params_t params;
	params.wnd_post_message.msg = *msg;
	IpcCall(NULL, SYS_WndPostMessage, &params);
}


bool __declspec(dllexport) WndAttachInputDevice(unsigned type, const wchar_t *device)
{
	wnd_params_t params;

	params.wnd_attach_input_device.type = type;
	if (!IpcCallExtra(NULL, SYS_WndAttachInputDevice, 
		&params, 
		device, 
		wcslen(device) * sizeof(*device)))
		return false;

    if (!IpcReceive(NULL, &params))
		return false;

    if (params.wnd_attach_input_device_reply.error == 0)
		return true;
	else
	{
		errno = params.wnd_attach_input_device_reply.error;
		return false;
	}
}


/**
 * Creates a window
 *
 * \param parent Window which will contain this one (for example, a top-level window to contain a control). Set to NULL to create 
 *	a top-level window.
 * \param attrs Array of attributes to assign to the new window
 * \param num_attrs Number of elements in the \p attrs array
 *
 * \return A new window handle if successful; NULL otherwise
 */
wnd_h __declspec(dllexport) WndCreate(wnd_h parent, const wnd_attr_t *attrs, unsigned num_attrs)
{
	wnd_params_t params;

	params.wnd_create.parent = parent;
	if (!IpcCallExtra(NULL, SYS_WndCreate, &params, attrs, num_attrs * sizeof(*attrs)))
		return 0;

	if (!IpcReceive(NULL, &params))
		return 0;

	return params.wnd_create_reply.wnd;
}


/**
 * Closes a window created using \p WndCreate
 *
 * \param wnd Handle to the window to be closed
 */
void __declspec(dllexport) WndClose(wnd_h wnd)
{
    wnd_params_t params;
	params.wnd_close.wnd = wnd;
	IpcCall(NULL, SYS_WndClose, &params);
}


/**
 * Posts a message in the calling thread's queue to signal its intention to exit the message loop
 *
 * \param code Return code from the message loop
 */
void __declspec(dllexport) WndPostQuitMessage(int code)
{
    wnd_params_t params;
	params.wnd_post_quit_message.code = code;
	IpcCall(NULL, SYS_WndPostQuitMessage, &params);
}


/**
 * Sets up a window for painting in response to a MSG_PAINT message and retrieves the window's graphics context
 *
 * \param hwnd Window to be painted
 * \param rect Pointer to an rect_t structure, which will receive the window's invalid rectangle
 *
 * \return A handle to the window's graphics context
 */
gfx_h __declspec(dllexport) WndBeginPaint(wnd_h hwnd, rect_t *rect)
{
    wnd_params_t params;

    params.wnd_begin_paint.wnd = hwnd;
	if (!IpcCall(NULL, SYS_WndBeginPaint, &params))
		return 0;

	if (!IpcReceive(NULL, &params))
		return 0;

    if (rect != NULL)
		*rect = params.wnd_begin_paint_reply.rect;

	return params.wnd_begin_paint_reply.gfx;
}


void __declspec(dllexport) WndInvalidate(wnd_h hwnd, const rect_t *rect)
{
    wnd_params_t params;

	params.wnd_invalidate.wnd = hwnd;
	if (rect == NULL)
		params.wnd_invalidate.rect.left = 
			params.wnd_invalidate.rect.top = 
			params.wnd_invalidate.rect.right = 
			params.wnd_invalidate.rect.bottom = 0;
	else
		params.wnd_invalidate.rect = *rect;

	IpcCall(NULL, SYS_WndInvalidate, &params);
}


bool __declspec(dllexport) WndGetAttribute(wnd_h hwnd, uint32_t code, wnd_data_t *data)
{
    wnd_params_t params;

	params.wnd_get_attribute.wnd = hwnd;
	params.wnd_get_attribute.code = code;
	if (!IpcCall(NULL, SYS_WndGetAttribute, &params))
		return false;

	if (!IpcReceive(NULL, &params))
		return false;

	if (params.wnd_get_attribute_reply.attr.code == 0)
		return false;
	else
	{
		*data = params.wnd_get_attribute_reply.attr.data;
		return true;
	}
}


void __declspec(dllexport) WndSetAttribute(wnd_h hwnd, uint32_t code, const wnd_data_t *data)
{
    wnd_params_t params;

	params.wnd_set_attribute.wnd = hwnd;
	params.wnd_set_attribute.attr.code = code;
	params.wnd_set_attribute.attr.data = *data;
	IpcCall(NULL, SYS_WndSetAttribute, &params);
}


bool __declspec(dllexport) WndHasFocus(wnd_h hwnd)
{
	wnd_params_t params;

	params.wnd_has_focus.wnd = hwnd;
	if (!IpcCall(NULL, SYS_WndHasFocus, &params))
		return false;

    if (!IpcReceive(NULL, &params))
		return false;

	return params.wnd_has_focus_reply.has_focus;
}


void __declspec(dllexport) WndSetFocus(wnd_h hwnd)
{
	wnd_params_t params;

	params.wnd_set_focus.wnd = hwnd;
    IpcCall(NULL, SYS_WndSetFocus, &params);
}


wnd_h __declspec(dllexport) WndWindowAtPoint(wnd_h root, int x, int y)
{
    wnd_params_t params;

	params.wnd_window_at_point.root = root;
	params.wnd_window_at_point.x = x;
	params.wnd_window_at_point.y = y;
	if (!IpcCall(NULL, SYS_WndWindowAtPoint, &params))
		return 0;

	if (!IpcReceive(NULL, &params))
		return 0;

	return params.wnd_window_at_point_reply.wnd;
}


wnd_h __declspec(dllexport) WndOwnRoot(const wnd_attr_t *attrs, unsigned num_attrs)
{
    wnd_params_t params;

    if (!IpcCallExtra(NULL, SYS_WndOwnRoot, NULL, attrs, sizeof(*attrs) * num_attrs))
		return 0;

    if (!IpcReceive(NULL, &params))
		return 0;

	return params.wnd_own_root_reply.wnd;
}


void __declspec(dllexport) WndSetCapture(wnd_h hwnd, bool set)
{
    wnd_params_t params;

	params.wnd_set_capture.wnd = hwnd;
	params.wnd_set_capture.set = set;
	IpcCall(NULL, SYS_WndSetCapture, &params);
}
