/* $Id$ */

#ifndef WINMGR_INTERNAL_H_
#define WINMGR_INTERNAL_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <gui/winmgr.h>
#include <os/framebuf.h>
#include <os/rtl.h>

typedef struct wmgr_client_t wmgr_client_t;
struct wmgr_client_t
{
    wmgr_client_t *prev, *next;
    handle_t ipc;
	wmgr_shared_t *shared;
    struct wnd_message_t *messages;
    unsigned num_messages;
    lmutex_t mux_messages;
    handle_t has_message;
    int last_mouse_move_message;
};

typedef struct gmgr_surface_t gmgr_surface_t;
struct gmgr_surface_t
{
	surface_t surf;
	int refs;
	uint32_t flags;
	void *base;
	videomode_t mode;
	handle_t device;
};


typedef struct clip_t clip_t;
struct clip_t
{
    unsigned num_rects;
    rect_t *rects;
};


typedef struct gfx_t gfx_t;
struct gfx_t
{
	lmutex_t mux;
	gmgr_surface_t *surface;
    clip_t clip;
    colour_t colour_fill;
    colour_t colour_pen;
};


typedef struct wnd_t wnd_t;
struct wnd_t
{
    wnd_t *prev, *next;
    wnd_h handle;
    lmutex_t mux;
    wnd_attr_t *attrs;
    unsigned num_attrs;
    rect_t invalid;
    wmgr_client_t *client;
    wnd_h parent;
    wnd_t *child_first, *child_last;
    gfx_t gfx;
    bool is_painting;
};

wmgr_client_t *WmgrGetClient(void);
void *WmgrGetThreadParam(void);
void WmgrQueueKeyboardInput(uint32_t key);
void WmgrQueueMouseInput(unsigned event, int x, int y);
bool WmgrDispatchMessage(wmgr_client_t *client, wnd_message_t *msg);
wnd_t *WmgrLockWindow(wnd_h hwnd);
void WmgrUnlockWindow(wnd_t *wnd);
bool WmgrGetPosition(wnd_t *wnd, rect_t *rect);
void WmgrUpdateClip(gfx_t *gfx, wnd_t* wnd);
void WmgrGetMessage(void);
void WmgrPostMessage(const wnd_message_t *msg);
void WmgrSetCursorPos(const point_t *pt);
void WmgrShowCursor(bool show);

int ClipIntersect(const rect_t* pos, const rect_t *Clip);

bool RectTestOverlap(const rect_t *a, const rect_t *b);
bool RectIsEmpty(const rect_t *r);
void RectOffset(rect_t *r, int dx, int dy);
void RectUnion(rect_t *dest, const rect_t *a, const rect_t *b);

gmgr_surface_t *GmgrCreateDeviceSurface(handle_t device, const videomode_t *mode);
gmgr_surface_t *GmgrCreateMemorySurface(const videomode_t *mode, uint32_t flags);
gmgr_surface_t *GmgrCopySurface(gmgr_surface_t *surf);
void GmgrCloseSurface(gmgr_surface_t *surf);

#define GMGR_SURFACE_INTERNAL	0
#define GMGR_SURFACE_SHARED		1

void GmgrInitDefaultGraphics(gfx_t *gfx);
gfx_t *GmgrLockGraphics(gfx_h hgfx);
void GmgrUnlockGraphics(gfx_t *gfx);
void GmgrToScreen(rect_t *dest, const rect_t *mgl);
void GmgrToScreenPoint(int *ax, int *ay, int x, int y);
void GmgrToVirtual(rect_t *dest, const rect_t *virt);
void GmgrToVirtualPoint(int *ax, int *ay, int x, int y);
void GmgrFlush(const void *ptr, size_t length);
gfx_h GmgrCreateBitmap(unsigned width, unsigned height, bitmap_desc_t *desc);
void GmgrClose(gfx_h hgfx);
void GmgrFinishedPainting(gfx_h hgfx);
void GmgrDrawCursor(int x, int y, bool draw);
void GmgrMoveCursor(int from_x, int from_y, int to_x, int to_y);

wnd_h WmgrCreate(wnd_h parent, const wnd_attr_t *attrs, unsigned num_attrs);
void WmgrClose(wnd_h wnd);
void WmgrPostQuitMessage(int code);
gfx_h WmgrBeginPaint(wnd_h wnd, rect_t *rect);
void WmgrInvalidate(wnd_h hwnd, const rect_t *rect);
bool WmgrGetAttribute(wnd_h hwnd, uint32_t code, wnd_data_t *data);
void WmgrSetAttribute(wnd_h hwnd, uint32_t code, const wnd_data_t *data);
bool WmgrHasFocus(wnd_h hwnd);
void WmgrSetFocus(wnd_h hwnd);
wnd_h WmgrWindowAtPoint(wnd_h root, int x, int y);
wnd_h WmgrOwnRoot(const wnd_attr_t *attrs, unsigned num_attrs);
void WmgrSetCapture(wnd_h hwnd, bool set);

void GmgrFillRect(gfx_h hgfx, const rect_t *rect);
void GmgrRectangle(gfx_h hgfx, const rect_t *rect);
void GmgrDrawText(gfx_h hgfx, const rect_t *rect, const wchar_t *str, size_t length);
void GmgrSetFillColour(gfx_h hgfx, colour_t clr);
void GmgrSetPenColour(gfx_h hgfx, colour_t clr);
void GmgrLine(gfx_h hgfx, int x1, int y1, int x2, int y2);
void GmgrBlt(gfx_h hdest, const rect_t *dest, gfx_h hsrc, const rect_t *src);
void GmgrGetTextSize(gfx_h hgfx, const wchar_t *str, size_t length, point_t *size);

extern point_t wmgr_cursor_pos;

extern lmutex_t gmgr_draw;
extern gmgr_surface_t *gmgr_screen;
extern font_t *gmgr_font;

#ifdef __cplusplus
}
#endif

#endif
