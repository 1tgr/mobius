/* $Id: winmgr.h,v 1.3 2002/08/17 23:09:01 pavlovskii Exp $ */

#ifndef __KERNEL_WINMGR_H
#define __KERNEL_WINMGR_H

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 *  \ingroup    kernel
 *  \defgroup   winmgr  Kernel-Mode Window Manager
 *  @{
 */

#include <os/gui.h>
#include <os/video.h>
#include <kernel/handle.h>

typedef struct wndattr_hdr_t wndattr_hdr_t;
struct wndattr_hdr_t
{
    wndattr_hdr_t *prev, *next;
    uint32_t id;
    uint32_t type;
    size_t size;
};

typedef struct window_t window_t;
struct window_t
{
    handle_hdr_t hdr;
    window_t *prev, *next;
    window_t *child_first, *child_last;
    window_t *child_focus, *parent;
    struct thread_t *owner;
    spinlock_t sem;
    wndattr_hdr_t *attr_first, *attr_last;
    MGLrect invalid_rect;
    MGLclip clip;
};

struct process_t;

wndattr_hdr_t *WndiGetAttribute(window_t *wnd, uint32_t id, uint32_t type);
#define     WndiGetAttributeData(attr, type)  ((type) ((attr) + 1))
bool        WndiGetPosition(window_t *wnd, MGLrect *pos);
void        WndiHasMessage(window_t *wnd);
void        WndiPostMessage(window_t *wnd, const msg_t *msg);
void        WndiInvalidate(window_t *wnd, const MGLrect *rect);
void        WndiInvalidateTree(window_t *wnd, window_t *except, 
                               const MGLrect *rect);
wchar_t *   WndiGetTitle(window_t *wnd);
void        WndiSetFocus(window_t *wnd);
window_t *  WndLockWindow(struct process_t *proc, handle_t hnd);
#define     WndUnlockWindow(proc, hnd)  HndUnlock(proc, hnd, 'wndo')
window_t *  WndiGetFocusedChild(window_t *wnd);
handle_t    WndiGetHandle(struct process_t *proc, window_t *wnd);
window_t *  WndiWindowAtPoint(window_t *root, MGLreal x, MGLreal y);
bool        WndiHasFocus(window_t *wnd);
void        WndiUpdateClip(window_t *wnd);
int         ClipIntersect(const MGLrect* pos, const MGLrect *Clip);

extern window_t wnd_root, *wnd_focus, *wnd_capture;

/*! @} */

#ifdef __cplusplus
}
#endif

#endif
