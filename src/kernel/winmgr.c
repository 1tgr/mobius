/* $Id: winmgr.c,v 1.5 2002/08/17 19:13:32 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/proc.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/winmgr.h>

/*#define DEBUG*/
#include <kernel/debug.h>

#include <os/syscall.h>

#include <errno.h>
#include <stdio.h>

window_t wnd_root = { { 0, 'wndo', NULL } };
window_t *wnd_focus = &wnd_root;
window_t *wnd_capture;
static spinlock_t sem_focus;

wndattr_hdr_t *WndiGetAttribute(window_t *wnd, uint32_t id, uint32_t type)
{
    wndattr_hdr_t *attr;
    
    for (attr = wnd->attr_first; attr != NULL; attr = attr->next)
        if (attr->id == id &&
            attr->type == type)
            return attr;
    
    return NULL;
}

bool WndiGetPosition(window_t *wnd, MGLrect *pos)
{
    wndattr_hdr_t *attr;
    attr = WndiGetAttribute(wnd, WND_ATTR_POSITION, WND_TYPE_RECT);
    if (attr == NULL)
    {
        pos->left = pos->top = pos->right = pos->bottom = 0;
        return false;
    }
    else
    {
        *pos = *WndiGetAttributeData(attr, MGLrect*);
        return true;
    }
}

void WndiHasMessage(window_t *wnd)
{
    EvtSignal(wnd->owner->process, wnd->owner->info->msgqueue_event);
}

void WndiPostMessage(window_t *wnd, const msg_t *msg)
{
    msg_t temp;
    temp = *msg;
    temp.window = (handle_t) wnd;
    QueueAppend(&wnd->owner->msgqueue, &temp, sizeof(temp));
    WndiHasMessage(wnd);
}

void WndiInvalidate(window_t *wnd, const MGLrect *rect)
{
    MGLrect pos, r;
    bool was_empty;
    int isect;

    WndiGetPosition(wnd, &pos);
    if (rect == NULL)
        r = pos;
    else
    {
        r = *rect;

        if (r.left < pos.left)
            r.left = pos.left;
        if (r.top < pos.top)
            r.top = pos.top;
        if (r.right > pos.right)
            r.right = pos.right;
        if (r.bottom > pos.bottom)
            r.bottom = pos.bottom;

        if (r.right <= r.left ||
            r.bottom <= r.top)
            return;

        isect = ClipIntersect(&r, &pos);
        if ((isect & 0xf0) == 0x50 ||
            (isect & 0x0f) == 0x05)
            return;
    }

    was_empty = wnd->invalid_rect.right - wnd->invalid_rect.left == 0 &&
        wnd->invalid_rect.bottom - wnd->invalid_rect.top == 0;

    if (was_empty)
        wnd->invalid_rect = r;
    else
    {
        wnd->invalid_rect.left = 
            min(wnd->invalid_rect.left, r.left);
	wnd->invalid_rect.top = 
            min(wnd->invalid_rect.top, r.top);
	wnd->invalid_rect.right = 
            max(wnd->invalid_rect.right, r.right);
	wnd->invalid_rect.bottom = 
            max(wnd->invalid_rect.bottom, r.bottom);
    }

    if (was_empty && 
        !(wnd->invalid_rect.right - wnd->invalid_rect.left == 0 &&
        wnd->invalid_rect.bottom - wnd->invalid_rect.top == 0))
    {
        msg_t msg;
        msg.code = MSG_PAINT;
        WndiPostMessage(wnd, &msg);
    }
}

void WndiInvalidateTree(window_t *wnd, window_t *except, const MGLrect *rect)
{
    window_t *child;

    if (wnd != except)
    {
        WndiInvalidate(wnd, rect);
        for (child = wnd->child_first; child != NULL; child = child->next)
            WndiInvalidateTree(child, except, rect);
    }
}

wchar_t *WndiGetTitle(window_t *wnd)
{
    wndattr_hdr_t *attr;
    attr = WndiGetAttribute(wnd, WND_ATTR_TITLE, WND_TYPE_STRING);
    if (attr == NULL)
        return NULL;
    else
        return WndiGetAttributeData(attr, wchar_t*);
}

void WndiSetFocus(window_t *wnd)
{
    window_t *parent, *child, *temp;
    msg_t msg;

    msg.code = MSG_BLUR;
    temp = wnd_focus;
    while (temp != NULL)
    {
        WndiPostMessage(temp, &msg);
        temp = temp->parent;
    }
    
    SpinAcquire(&sem_focus);
    wnd_focus = wnd;
    parent = wnd->parent;
    child = wnd;
    while (parent != NULL)
    {
        SpinAcquire(&parent->sem);
        parent->child_focus = child;
        SpinRelease(&parent->sem);
        child = parent;
        parent = parent->parent;
    }

    SpinRelease(&sem_focus);

    msg.code = MSG_FOCUS;
    temp = wnd_focus;
    while (temp != NULL)
    {
        WndiPostMessage(temp, &msg);
        temp = temp->parent;
    }
}

window_t *WndLockWindow(process_t *proc, handle_t hnd)
{
    handle_hdr_t *ptr;
    ptr = HndLock(proc, hnd, 'wndo');
    if (ptr == NULL)
        return NULL;
    else
        return (window_t*) (ptr - 1);
}

handle_t WndCreate(handle_t parent, const wndattr_t *attribs, unsigned num_attribs)
{
    window_t *wnd, *par;
    wndattr_hdr_t *attr;
    handle_t hnd;
    unsigned i;

    TRACE3("WndCreate(%d, %p, %u)\n", parent, attribs, num_attribs);
    if (!MemVerifyBuffer(attribs, num_attribs * sizeof(wndattr_t)))
    {
        errno = EBUFFER;
        return NULL;
    }

    if (parent == NULL)
        par = &wnd_root;
    else
    {
        par = WndLockWindow(NULL, parent);
        if (par == NULL)
        {
            errno = EHANDLE;
            return NULL;
        }

        WndUnlockWindow(NULL, parent);
    }

    wnd = malloc(sizeof(window_t));
    if (wnd == NULL)
        return NULL;

    memset(wnd, 0, sizeof(window_t));
    HndInit(&wnd->hdr, 'wndo');
    SpinInit(&wnd->sem);
    wnd->owner = current();
    wnd->parent = par;

    for (i = 0; i < num_attribs; i++)
    {
        if (WND_TYPE_IS_DIRECT(attribs[i].type))
        {
            attr = malloc(sizeof(wndattr_hdr_t) + sizeof(addr_t));
            *(addr_t*) (attr + 1) = attribs[i].data.addr;
            TRACE2("attrib %d: direct = %x\n", i, attribs[i].data.ui32);
        }
        else
        {
            if (!MemVerifyBuffer(attribs[i].data.ptr, attribs[i].size))
            {
                errno = EBUFFER;
                free(wnd);
                return NULL;
            }

            attr = malloc(sizeof(wndattr_hdr_t) + attribs[i].size);
            memcpy(attr + 1, attribs[i].data.ptr, attribs[i].size);
            TRACE2("attrib %d: buffered (%u bytes)\n", i, attribs[i].size);
        }

        attr->id = attribs[i].id;
        attr->type = attribs[i].type;
        attr->size = attribs[i].size;
        
        LIST_ADD(wnd->attr, attr);
    }

    WndiSetFocus(wnd);
    
    SpinAcquire(&par->sem);
    LIST_ADD(par->child, wnd);
    SpinRelease(&par->sem);

    hnd = HndDuplicate(NULL, &wnd->hdr);
    WndInvalidate(hnd, NULL);
    return hnd;
}

window_t *WndiGetFocusedChild(window_t *wnd)
{
    while (wnd->child_focus != NULL)
        wnd = wnd->child_focus;
    return wnd;
}

bool WndClose(handle_t hnd)
{
    window_t *wnd, *new_focus;
    msg_t *msg;
    unsigned i;
    MGLrect rect;
    wndattr_hdr_t *attr, *next;

    TRACE1("WndClose(%d)\n", hnd);
    wnd = WndLockWindow(NULL, hnd);
    if (wnd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    LIST_REMOVE(wnd->parent->child, wnd);

    if (wnd_focus == wnd)
    {
        window_t *parent;

        if (wnd->parent->child_focus == wnd)
        {
            wnd->parent->child_focus = wnd->next;
            if (wnd->parent->child_focus == NULL)
                wnd->parent->child_focus = wnd->parent->child_first;
        }

        new_focus = wnd->next;
        parent = wnd->parent;
        while (new_focus == NULL)
        {
            if (parent->child_first == NULL)
            {
                parent = parent->parent;
                if (parent->child_focus == NULL)
                    new_focus = NULL;
                else
                    new_focus = WndiGetFocusedChild(parent);
            }
            else
                new_focus = WndiGetFocusedChild(parent->child_first);
        }

        WndiSetFocus(new_focus);
    }

    msg = QUEUE_DATA(wnd->owner->msgqueue);
    for (i = 0; i < QUEUE_COUNT(wnd->owner->msgqueue, msg_t); i++)
        if ((window_t*) msg[i].window == wnd)
            msg[i].window = NULL;

    WndiGetPosition(wnd, &rect);
    WndiInvalidateTree(&wnd_root, wnd, &rect);

    for (attr = wnd->attr_first; attr != NULL; attr = next)
    {
        next = attr->next;
        free(attr);
    }

    WndUnlockWindow(NULL, hnd);
    return HndClose(NULL, hnd, 'wndo');
}

bool WndPostMessage(handle_t hnd, const msg_t *msg)
{
    window_t *wnd;
    
    if (!MemVerifyBuffer(msg, sizeof(msg_t)))
    {
        errno = EBUFFER;
        return false;
    }

    TRACE2("WndPostMessage(%d, %p)\n", hnd, msg);
    wnd = WndLockWindow(NULL, hnd);
    if (wnd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    WndiPostMessage(wnd, msg);
    WndUnlockWindow(NULL, hnd);
    return true;
}

bool WndInvalidate(handle_t hnd, const MGLrect *rect)
{
    window_t *wnd;

    TRACE2("WndInvalidate(%d, %p)\n", hnd, rect);
    wnd = WndLockWindow(NULL, hnd);
    if (wnd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    WndiInvalidate(wnd, rect);
    WndUnlockWindow(NULL, hnd);
    return true;
}

handle_t WndiGetHandle(process_t *proc, window_t *wnd)
{
    unsigned i;

    for (i = 0; i < proc->handle_count; i++)
        if (proc->handles[i] == &wnd->hdr)
            return i;
    
    wprintf(L"WndiGetHandle: duplicating window %p into process %d\n",
        wnd, proc->id);
    return HndDuplicate(proc, &wnd->hdr);
}

bool WndGetMessage(msg_t *msg)
{
    TRACE1("WndGetMessage(%p)\n", msg);

    if (!MemVerifyBuffer(msg, sizeof(msg_t)))
    {
        errno = EBUFFER;
        return false;
    }

    while (QUEUE_COUNT(current()->msgqueue, msg_t) > 0)
    {
        QueuePopFirst(&current()->msgqueue, msg, sizeof(msg_t));

        if (msg->window != NULL)
        {
            wchar_t *title;
            title = WndiGetTitle((window_t*) msg->window);
            msg->window = WndiGetHandle(current()->process, (window_t*) msg->window);
            /*wprintf(L"WndGetMessage: window = %d(%s) code = %x\n",
                msg->window, title, msg->code);*/

            if (msg->code == MSG_PAINT)
            {
                window_t *wnd;
                //unsigned i;

                wnd = WndLockWindow(NULL, msg->window);
                assert(wnd != NULL);
                WndiUpdateClip(wnd);
                wnd->invalid_rect.left = wnd->invalid_rect.top = 
                    wnd->invalid_rect.right = wnd->invalid_rect.bottom = 0;
                msg->p.params[0] = wnd->clip.num_rects;

                /*wprintf(L"Clipping for %p(%s):\n", wnd, WndiGetTitle(wnd));
                for (i = 0; i < wnd->clip.num_rects; i++)
                {
                    wprintf(L"\tclip %d: (%d,%d), (%d,%d)\n",
                        i, 
                        wnd->clip.rects[i].left, wnd->clip.rects[i].top, 
                        wnd->clip.rects[i].right, wnd->clip.rects[i].bottom);
                }*/

                WndUnlockWindow(NULL, msg->window);
            }

            /*if (QUEUE_COUNT(current()->msgqueue, msg_t) > 0)
                EvtSignal(NULL, current()->info->msgqueue_event);*/

            return true;
        }
    }

    TRACE0("WndGetMessage: no messages\n");
    return false;
}

bool WndGetAttribute(handle_t hnd, uint32_t id, uint32_t type, void *buf, size_t *size)
{
    wndattr_hdr_t *attr;
    window_t *wnd;

#ifdef DEBUG
    wprintf(L"WndGetAttribute(%d, %x, %x, %p, %p)\n", hnd, id, type, buf, size);
#endif

    if (!MemVerifyBuffer(size, sizeof(size_t) ||
        !MemVerifyBuffer(buf, *size)))
    {
        errno = EBUFFER;
        return false;
    }

    wnd = WndLockWindow(NULL, hnd);
    if (wnd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    attr = WndiGetAttribute(wnd, id, type);
    if (attr == NULL)
    {
        errno = ENOTFOUND;
        return false;
    }

    if (*size < attr->size)
    {
        *size = attr->size;
        errno = ENOMEM;
        return false;
    }

    *size = attr->size;
    memcpy(buf, WndiGetAttributeData(attr, void*), *size);
    TRACE2("WndGetAttribute: attrib = %x size = %u\n",
        *WndiGetAttributeData(attr, uint32_t*), *size);

    WndUnlockWindow(NULL, hnd);
    return true;
}

void WndiDisplaceWindow(window_t *wnd, MGLreal dx, MGLreal dy)
{
    MGLrect *rect;
    window_t *child;
    wndattr_hdr_t *attr;

    WndiInvalidate(wnd, NULL);

    if (dx != 0 || dy != 0)
    {
        for (child = wnd->child_first; child != NULL; child = child->next)
        {
            attr = WndiGetAttribute(child, WND_ATTR_POSITION, WND_TYPE_RECT);
            if (attr != NULL)
            {
                rect = WndiGetAttributeData(attr, MGLrect*);
                WndiInvalidateTree(&wnd_root, child, rect);
                rect->left += dx;
                rect->top += dy;
                rect->right += dx;
                rect->bottom += dy;
            }

            WndiDisplaceWindow(child, dx, dy);
        }
    }
}

bool WndSetAttribute(handle_t hnd, uint32_t id, uint32_t type, const void *buf, size_t size)
{
    wndattr_hdr_t *attr, *prev, *next;
    window_t *wnd;
    MGLreal dx, dy;

#ifdef DEBUG
    wprintf(L"WndSetAttribute(%d, %x, %x, %p, %u)\n", hnd, id, type, buf, size);
#endif

    if (!WND_TYPE_IS_DIRECT(type) &&
        !MemVerifyBuffer(buf, size))
    {
        errno = EBUFFER;
        return false;
    }

    wnd = WndLockWindow(NULL, hnd);
    if (wnd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    attr = WndiGetAttribute(wnd, id, type);
    if (attr == NULL)
    {
        attr = malloc(sizeof(wndattr_hdr_t));
        attr->id = id;
        attr->type = type;
        LIST_ADD(wnd->attr, attr);
        dx = dy = 0;
    }
    else if (id == WND_ATTR_POSITION && type == WND_TYPE_RECT)
    {
        const MGLrect *old, *new;
        old = WndiGetAttributeData(attr, const MGLrect*);
        new = (const MGLrect*) buf;
        dx = new->left - old->left;
        dy = new->top - old->top;
        if (dx != 0 || dy != 0)
            WndiInvalidateTree(&wnd_root, wnd, old);
    }
    
    prev = attr->prev;
    next = attr->next;

    if (WND_TYPE_IS_DIRECT(type))
    {
        attr = realloc(attr, sizeof(wndattr_hdr_t) + sizeof(addr_t));
        *(const void**) (attr + 1) = buf;
        TRACE1("attrib: direct = %p\n", buf);
    }
    else
    {
        attr = realloc(attr, sizeof(wndattr_hdr_t) + size);
        memcpy(attr + 1, buf, size);
        TRACE1("attrib: buffered (%u bytes)\n", size);
    }

    attr->size = size;
    if (prev == NULL)
        wnd->attr_first = attr;
    else
        prev->next = attr;

    if (next == NULL)
        wnd->attr_last = attr;
    else
        next->prev = attr;
    
    TRACE2("WndSetAttribute: attrib = %x size = %u\n",
        *WndiGetAttributeData(attr, uint32_t*), size);

    if (id == WND_ATTR_POSITION && type == WND_TYPE_RECT)
        WndiDisplaceWindow(wnd, dx, dy);

    WndUnlockWindow(NULL, hnd);
    return true;
}

handle_t WndOwnRoot(void)
{
    if (wnd_root.owner != NULL)
    {
        errno = EACCESS;
        return NULL;
    }

    wnd_root.owner = current();
    return HndDuplicate(NULL, &wnd_root.hdr);
}

window_t *WndiWindowAtPoint(window_t *root, MGLreal x, MGLreal y)
{
    wndattr_hdr_t *attr;
    MGLrect *pos;
    window_t *ret, *child;
    
    for (child = root->child_last; child != NULL; child = child->prev)
    {
        ret = WndiWindowAtPoint(child, x, y);
        if (ret != NULL)
            return ret;
    }

    attr = WndiGetAttribute(root, WND_ATTR_POSITION, WND_TYPE_RECT);
    if (attr == NULL)
        return NULL;

    pos = WndiGetAttributeData(attr, MGLrect*);
    if (x >= pos->left &&
        y >= pos->top &&
        x < pos->right &&
        y < pos->bottom)
        return root;
    
    return NULL;
}

bool WndQueueInput(const wndinput_t *data)
{
    msg_t msg;
    window_t *wnd;

    if (!MemVerifyBuffer(data, sizeof(wndinput_t)))
    {
        errno = EBUFFER;
        return false;
    }

    switch (data->type)
    {
    case WND_INPUT_KEY_DOWN:
    case WND_INPUT_KEY_UP:
        msg.code = data->type == WND_INPUT_KEY_DOWN ? MSG_KEYDOWN : MSG_KEYUP;
        msg.p.key = data->data.key;
        if (wnd_focus != NULL)
            WndiPostMessage(wnd_focus, &msg);
        break;

    case WND_INPUT_MOUSE_DOWN:
    case WND_INPUT_MOUSE_UP:
        msg.code = data->type == WND_INPUT_MOUSE_DOWN ? MSG_MOUSEDOWN : MSG_MOUSEUP;
        msg.p.mouse.buttons = data->data.mouse_down.buttons;
        msg.p.mouse.x = data->data.mouse_down.x;
        msg.p.mouse.y = data->data.mouse_down.y;
        if (wnd_capture != NULL)
            wnd = wnd_capture;
        else
            wnd = WndiWindowAtPoint(&wnd_root, 
                data->data.mouse_down.x, data->data.mouse_down.y);
        if (wnd != NULL)
            WndiPostMessage(wnd, &msg);
        break;

    case WND_INPUT_MOUSE_MOVE:
        msg.code = MSG_MOUSEMOVE;
        msg.p.mouse.buttons = 0;
        msg.p.mouse.x = data->data.mouse_move.x;
        msg.p.mouse.y = data->data.mouse_move.y;
        if (wnd_capture != NULL)
            wnd = wnd_capture;
        else
            wnd = WndiWindowAtPoint(&wnd_root, 
                data->data.mouse_move.x, data->data.mouse_move.y);
        if (wnd != NULL)
            WndiPostMessage(wnd, &msg);
        break;

    case WND_INPUT_MOUSE_WHEEL:
        msg.code = MSG_MOUSEWHEEL;
        msg.p.wheel.delta = data->data.mouse_wheel.dwheel;
        msg.p.wheel.x = data->data.mouse_wheel.x;
        msg.p.wheel.y = data->data.mouse_wheel.y;
        if (wnd_capture != NULL)
            wnd = wnd_capture;
        else
            wnd = WndiWindowAtPoint(&wnd_root, 
                data->data.mouse_wheel.x, data->data.mouse_wheel.y);
        if (wnd != NULL)
            WndiPostMessage(wnd, &msg);
        break;
    }

    return true;
}

bool WndiHasFocus(window_t *wnd)
{
    if (wnd_focus == wnd)
        return true;

    for (wnd = wnd->child_first; wnd != NULL; wnd = wnd->next)
        if (WndiHasFocus(wnd))
            return true;

    return false;
}

bool WndSetFocus(handle_t hnd)
{
    window_t *wnd;

    wnd = WndLockWindow(NULL, hnd);
    if (wnd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    if (!WndiHasFocus(wnd))
        WndiSetFocus(wnd);

    WndUnlockWindow(NULL, hnd);
    return true;
}

bool WndHasFocus(handle_t hnd)
{
    window_t *wnd;
    bool ret;

    wnd = WndLockWindow(NULL, hnd);
    if (wnd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    ret = WndiHasFocus(wnd);
    WndUnlockWindow(NULL, hnd);
    return ret;
}

bool WndSetCapture(handle_t hnd, bool set)
{
    window_t *wnd;
    
    wnd = WndLockWindow(NULL, hnd);
    if (wnd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    if (set && wnd_capture == NULL)
        wnd_capture = wnd;
    else if (!set && wnd_capture == wnd)
        wnd_capture = NULL;
    else
    {
        errno = EACCESS;
        WndUnlockWindow(NULL, hnd);
        return false;
    }

    WndUnlockWindow(NULL, hnd);
    return true;
}

bool WndGetClip(handle_t hnd, MGLrect *rects, size_t *size)
{
    window_t *wnd;

    if (!MemVerifyBuffer(size, sizeof(size_t)) ||
        !MemVerifyBuffer(rects, *size))
    {
        errno = EBUFFER;
        return false;
    }

    wnd = WndLockWindow(NULL, hnd);
    if (wnd == NULL)
    {
        errno = EHANDLE;
        return false;
    }

    if (*size < wnd->clip.num_rects * sizeof(MGLrect))
    {
        *size = wnd->clip.num_rects * sizeof(MGLrect);
        errno = ENOMEM;
        return false;
    }

    memcpy(rects, wnd->clip.rects, 
        min(wnd->clip.num_rects * sizeof(MGLrect), *size));
    *size = wnd->clip.num_rects * sizeof(MGLrect);

    WndUnlockWindow(NULL, hnd);
    return true;
}
