/* $Id$ */

#ifndef WINMGR_H_
#define WINMGR_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <os/video.h>

#ifndef GUI_EXPORT
#ifdef LIBGUI_EXPORTS
#define GUI_EXPORT  __declspec(dllexport)
#else
#define GUI_EXPORT
#endif
#endif

/*!
 *	\addtogroup	gui	GUI
 *	@{
 */

#ifdef __cplusplus
namespace OS
{
#endif

typedef unsigned long wnd_h;
typedef unsigned long gfx_h;

typedef union wnd_data_t wnd_data_t;
union wnd_data_t
{
	rect_t position;
	void *client_data;
};


typedef struct wnd_attr_t wnd_attr_t;
struct wnd_attr_t
{
    uint32_t code;
	wnd_data_t data;
};


#define WND_ATTR_POSITION       'posn'
#define WND_ATTR_CLIENT_DATA    'cdta'


typedef struct wmgr_shared_t wmgr_shared_t;
struct wmgr_shared_t
{
	wchar_t message_event_name[20];
};


typedef struct wmgr_global_t wmgr_global_t;
struct wmgr_global_t
{
	videomode_t video_mode;
};

#define WMGR_GLOBAL_NAME		L"_winmgr_global"

#define MSG_NONE                0
#define MSG_QUIT                1
#define MSG_PAINT               2
#define MSG_KEYDOWN             3
#define MSG_KEYUP               4
#define MSG_MOUSEMOVE           5
#define MSG_MOUSEDOWN           6
#define MSG_MOUSEUP             7
#define MSG_CLOSE               8
#define MSG_FOCUS               9
#define MSG_BLUR                10

#define SYS_WndConnect          0x1100
#define SYS_WndCreate           0x1101
#define SYS_WndClose            0x1102
#define SYS_WndPostQuitMessage  0x1103
#define SYS_WndBeginPaint       0x1104
#define SYS_WndInvalidate       0x1105
#define SYS_WndGetAttribute     0x1106
#define SYS_WndSetAttribute     0x1107
#define SYS_WndHasFocus         0x1108
#define SYS_WndSetFocus         0x1109
#define SYS_WndWindowAtPoint    0x110a
#define SYS_WndOwnRoot          0x110b
#define SYS_WndSetCapture       0x110c
#define SYS_WndDisconnect       0x110d
#define SYS_WndGetMessage       0x110e
#define SYS_WndPostMessage      0x110f
#define SYS_WndAttachInputDevice 0x1110

#define SYS_GfxFlush            0x1200
#define SYS_GfxCreateBitmap		0x1201
#define SYS_GfxClose			0x1202
#define SYS_GfxGetTextSize		0x1203

#define GFX_CMD_NOP             0
#define GFX_CMD_FILL_RECT       1
#define GFX_CMD_RECTANGLE       2
#define GFX_CMD_DRAW_TEXT       3
#define GFX_CMD_SET_FILL_COLOUR 4
#define GFX_CMD_SET_PEN_COLOUR  5
#define GFX_CMD_LINE            6
#define GFX_CMD_BLT				7

typedef struct gfx_command_t gfx_command_t;
struct gfx_command_t
{
    uint32_t command;
    uint32_t size;
    gfx_h hgfx;
    union
    {
        rect_t fill_rect, rectangle;
        struct
        {
            rect_t rect;
            wchar_t str[0];
        } draw_text;
        colour_t set_fill_colour, set_pen_colour;
        struct
        {
            int x1;
            int y1;
            int x2;
            int y2;
        } line;
		struct
		{
			gfx_h hsrc;
			rect_t src, dest;
		} blt;
    } u;
};

typedef struct bitmap_desc_t bitmap_desc_t;
struct bitmap_desc_t
{
	unsigned width;
	unsigned height;
	int pitch;
	unsigned bits_per_pixel;
	wchar_t area_name[20];
};


typedef struct wnd_thread_t wnd_thread_t;
struct wnd_thread_t
{
    handle_t ipc;
    handle_t shared_handle;
    handle_t got_message;
    wmgr_shared_t *shared;
};


typedef struct wnd_message_t wnd_message_t;
struct wnd_message_t
{
	wnd_h wnd;
	uint32_t code;
	void *client_data;

	union
	{
		struct
		{
			uint32_t key;
		} key_down, key_up;

		struct
		{
			int x;
			int y;
		} mouse_move, mouse_down, mouse_up;

		struct
		{
			int code;
		} quit;

		uint32_t generic[3];
	} params;
};


typedef union wnd_params_t wnd_params_t;
union wnd_params_t
{
	struct
	{
		wchar_t shared_area[16];
	} wnd_connect_reply;

	struct
	{
		wnd_h parent;
	} wnd_create;

	struct
	{
		wnd_h wnd;
	} wnd_begin_paint, wnd_has_focus, wnd_set_focus;

	struct
	{
		wnd_h wnd;
	} wnd_create_reply, wnd_own_root_reply, wnd_window_at_point_reply;

	struct
	{
		bool has_focus;
	} wnd_has_focus_reply;

	struct
	{
		wnd_h wnd;
	} wnd_close;

	struct
	{
		int code;
	} wnd_post_quit_message;

	struct
	{
		gfx_h gfx;
		rect_t rect;
	} wnd_begin_paint_reply;

	struct
	{
		wnd_h wnd;
		rect_t rect;
	} wnd_invalidate;

	struct
	{
		wnd_h wnd;
		uint32_t code;
	} wnd_get_attribute;

	struct
	{
		wnd_attr_t attr;
	} wnd_get_attribute_reply;

	struct
	{
		wnd_h wnd;
		wnd_attr_t attr;
	} wnd_set_attribute;

	struct
	{
		wnd_h root;
		int x;
		int y;
	} wnd_window_at_point;

	struct
	{
		wnd_h wnd;
		bool set;
	} wnd_set_capture;

	struct
	{
		unsigned type;
	} wnd_attach_input_device;

	struct
	{
		int error;
	} wnd_attach_input_device_reply;

	struct
	{
		wnd_message_t msg;
	} wnd_get_message_reply, wnd_post_message;

	struct
	{
		unsigned width;
		unsigned height;
	} gfx_create_bitmap;

	struct
	{
		gfx_h gfx;
		bitmap_desc_t desc;
	} gfx_create_bitmap_reply;

	struct
	{
		gfx_h gfx;
	} gfx_close, gfx_get_text_size;

	struct
	{
		point_t size;
	} gfx_get_text_size_reply;
};

/*!
 *	\brief	Connects to the window manager
 *
 *	\note	This internal function is called automatically for each thread by 
 *		GUI.DLL.
 */
bool WndConnectIpc(void);

/*!
 *	\brief	Disconnects from the window manager
 *
 *	\note	This internal function is called automatically for each thread by 
 *		GUI.DLL.
 */
void WndDisconnectIpc(void);

/*!
 *	\brief	Retrieves the thread's message event
 *
 *	The message event is signalled by the window manager when at least one 
 *		message is available for the thread. This handle can be used to wait for
 *		a GUI message at the same time as some other notification.
 *
 *	\return	A handle to the thread's message event
 */
handle_t WndGetMessageEvent(void);

/*!
 *	\brief	Retrieves a pointer to the thread's GUI data
 *
 *	GUI.DLL maintains a structure for each GUI thread. This function retrieves a
 *		pointer to this structure.
 *
 *	\return	A pointer to the thread's GUI data
 */
wnd_thread_t *WndGetThread(void);

void WndGetScreenSize(point_t *size);


/*!
 *	\brief	Creates a window
 *
 *	Creates a window with the specified attribute list. Include the 
 *		\b WND_ATTR_POSITION attribute to specify the window's position.
 *		Include the \b WND_ATTR_CLIENT_DATA attribute to associate some value with
 *		the window (for example, an object pointer).
 *
 *	\param	parent	Parent of the new window, or \b NULL to create a top-level 
 *		window
 *	\param	attrs	Pointer to an array of \b wnd_attr_t structures, which specify
 *		the initial attributes for the window
 *	\param	num_attrs	Number of elements in the \b attrs array
 *	\return	A handle to the new window, if successful, or \b NULL otherwise
 */
wnd_h WndCreate(wnd_h parent, const wnd_attr_t *attrs, unsigned num_attrs);

/*!
 *	\brief	Closes a window
 *
 *	A \b MSG_CLOSE message is posted to the thread after the window is closed
 *
 *	\param	wnd	Window to close
 */
void WndClose(wnd_h wnd);

/*!
 *	\brief	Posts a \b MSG_QUIT message to the thread
 *
 *	Commonly used to notify the message loop that the thread, or 
 *		application, should quit.
 *
 *	\param	code	Exit code to be included in the \b MSG_QUIT message
 */
void WndPostQuitMessage(int code);

/*!
 *	\brief	Begins repainting a window
 *
 *	Usually called in response to a \b MSG_PAINT message. This function 
 *		retrieves the window's current invalid rectangle, then clears the invalid
 *		rectangle and returns a handle to the window's graphics object.
 *
 *	\param	wnd	Handle to the window to repaint
 *	\param	rect	Pointer to an \b rect_t structure which will receive the 
 *		window's invalid rectangle, or \b NULL if this is not needed.
 *	\return	A graphics handle
 */
gfx_h WndBeginPaint(wnd_h wnd, rect_t *rect);
void WndInvalidate(wnd_h wnd, const rect_t *rect);
bool WndGetAttribute(wnd_h wnd, uint32_t attr, wnd_data_t *data);
void WndSetAttribute(wnd_h wnd, uint32_t attr, const wnd_data_t *data);
bool WndHasFocus(wnd_h wnd);
void WndSetFocus(wnd_h wnd);
wnd_h WndWindowAtPoint(wnd_h root, int x, int y);
wnd_h WndOwnRoot(const wnd_attr_t *attrs, unsigned num_attrs);
void WndSetCapture(wnd_h wnd, bool set);
bool WndGetMessage(wnd_message_t *msg);
void WndPostMessage(const wnd_message_t *msg);
bool WndAttachInputDevice(unsigned type, const wchar_t *device);

void GfxFillRect(gfx_h hgfx, const rect_t *rect);
void GfxRectangle(gfx_h hgfx, const rect_t *rect);
void GfxDrawText(gfx_h hgfx, const rect_t *rect, const wchar_t *str, size_t length);
void GfxSetFillColour(gfx_h hgfx, colour_t clr);
void GfxSetPenColour(gfx_h hgfx, colour_t clr);
void GfxLine(gfx_h hgfx, int x1, int y1, int x2, int y2);
void GfxBlt(gfx_h hdest, const rect_t *dest, gfx_h hsrc, const rect_t *src);
void GfxFlush(void);
gfx_h GfxCreateBitmap(unsigned width, unsigned height, bitmap_desc_t *desc);
void GfxClose(gfx_h hgfx);
void GfxGetTextSize(gfx_h hgfx, const wchar_t *str, size_t length, point_t *size);

#ifdef __cplusplus
}
}
#endif

/*! @} */

#endif
