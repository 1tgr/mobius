#ifndef __GUI_WINSERVE_H
#define __GUI_WINSERVE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <os/com.h>
#include <gui/surface.h>
#include <gui/font.h>

typedef struct msg_t msg_t;

#undef INTERFACE
#define INTERFACE	IWindow
DECLARE_INTERFACE(IWindow)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD_(dword, GetAttrib)(THIS_ dword index) PURE;
	STDMETHOD_(ISurface*, GetSurface)(THIS) PURE;
	STDMETHOD(InvalidateRect)(THIS_ const rectangle_t* rect) PURE;
	STDMETHOD(UpdateWindow)(THIS) PURE;
	STDMETHOD(DefWndProc)(THIS_ dword message, dword param) PURE;
	STDMETHOD_(IWindow*, GetFirstChild)(THIS) PURE;
	STDMETHOD(ClientToScreen)(THIS_ rectangle_t* rect) PURE;
};

struct msg_t
{
	IWindow* wnd;
	dword message;
	dword params;
};

dword IWindow_GetAttrib(IWindow* ptr, dword index);
ISurface* IWindow_GetSurface(IWindow* ptr);
HRESULT IWindow_InvalidateRect(IWindow* ptr, const rectangle_t* rect);
HRESULT IWindow_UpdateWindow(IWindow* ptr);
HRESULT IWindow_DefWndProc(IWindow* ptr, dword message, dword param);
IWindow* IWindow_GetFirstChild(IWindow* ptr);
HRESULT IWindow_ClientToScreen(IWindow* ptr, rectangle_t* rect);

// {816B7D58-910A-4187-AEF6-F30EFEFE4AEE}
DEFINE_GUID(IID_IWindow, 
0x816b7d58, 0x910a, 0x4187, 0xae, 0xf6, 0xf3, 0xe, 0xfe, 0xfe, 0x4a, 0xee);

typedef struct windowdef_t windowdef_t;
struct windowdef_t
{
	size_t size;
	dword flags;
	const wchar_t* title;
	size_t title_max;
	int x, y, width, height;
	IWindow* parent;
	HRESULT (*wndproc) (IWindow*, dword, dword);
};

#define WIN_TITLE		0x01
#define WIN_X			0x02
#define WIN_Y			0x04
#define WIN_WIDTH		0x08
#define WIN_HEIGHT		0x10
#define WIN_PARENT		0x20
#define WIN_WNDPROC		0x40

#define ATTR_WNDPROC	0
#define ATTR_PROCESS	1
#define ATTR_MSGQUEUE	2

#undef INTERFACE
#define INTERFACE	IMsgQueue
DECLARE_INTERFACE(IMsgQueue)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD(PeekMessage)(THIS_ msg_t* msg, bool remove) PURE;
	STDMETHOD(GetMessage)(THIS_ msg_t* msg) PURE;
	STDMETHOD(DispatchMessage)(THIS_ const msg_t* msg) PURE;
	STDMETHOD(PostMessage)(THIS_ IWindow* wnd, dword message, dword params) PURE;
};

// {816B7D5b-910A-4187-AEF6-F30EFEFE4AEE}
DEFINE_GUID(IID_IMsgQueue, 
0x816b7d5b, 0x910a, 0x4187, 0xae, 0xf6, 0xf3, 0xe, 0xfe, 0xfe, 0x4a, 0xee);

HRESULT IMsgQueue_PeekMessage(IMsgQueue* ptr, msg_t* msg, bool remove);
HRESULT IMsgQueue_GetMessage(IMsgQueue* ptr, msg_t* msg);
HRESULT IMsgQueue_DispatchMessage(IMsgQueue* ptr, const msg_t* msg);
HRESULT IMsgQueue_PostMessage(IMsgQueue* ptr, IWindow* wnd, dword message, dword params);

#undef INTERFACE
#define INTERFACE	IWindowServer
DECLARE_INTERFACE(IWindowServer)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void ** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	STDMETHOD_(IWindow*, CreateWindow)(THIS_ const windowdef_t* def) PURE;
	STDMETHOD_(ISurface*, GetScreen)(THIS) PURE;
	STDMETHOD_(IFont*, GetFont)(THIS_ int index) PURE;
};

// {816B7D57-910A-4187-AEF6-F30EFEFE4AEE}
DEFINE_GUID(IID_IWindowServer, 
0x816b7d57, 0x910a, 0x4187, 0xae, 0xf6, 0xf3, 0xe, 0xfe, 0xfe, 0x4a, 0xee);

IWindowServer* OpenServer();

IWindow* IWindowServer_CreateWindow(IWindowServer* ptr, const windowdef_t* def);
ISurface* IWindowServer_GetScreen(IWindowServer* ptr);
IFont* IWindowServer_GetFont(IWindowServer* ptr, int index);

#ifdef __cplusplus
}
#endif

#endif