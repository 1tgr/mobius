/* $Id: sysdef.h,v 1.2 2002/12/18 23:54:44 pavlovskii Exp $ */

/*!
 *    \ingroup    libsys
 *    \defgroup    sys    Syscall Interface
 *    @{
 */

#ifndef SYS_BEGIN_GROUP_GUI
#define SYS_BEGIN_GROUP_GUI(n)
#endif

#ifndef SYS_END_GROUP_GUI
#define SYS_END_GROUP_GUI(n)
#endif

#ifndef SYSCALL_BEGIN
#define SYSCALL_BEGIN(rtn, name, argbytes, args)    SYSCALL_GUI(rtn, name, argbytes, args)
#endif

#ifndef PARAM_LONG
#define PARAM_LONG(name)
#endif

#ifndef PARAM_ARRAY
#define PARAM_ARRAY(name, size, count)
#endif

#ifndef PARAM_OUT
#define PARAM_OUT(name, size)
#endif

#ifndef SYSCALL_END
#define SYSCALL_END(name)
#endif

#define SYS_WndCreate       0x1100
#define SYS_WndClose        0x1101
#define SYS_WndPostMessage  0x1102
#define SYS_WndInvalidate   0x1103
#define SYS_WndGetMessage   0x1104
#define SYS_WndGetAttribute 0x1105
#define SYS_WndSetAttribute 0x1106
#define SYS_WndOwnRoot      0x1107
#define SYS_WndQueueInput   0x1108
#define SYS_WndSetFocus     0x1109
#define SYS_WndHasFocus     0x110a
#define SYS_WndSetCapture   0x110b
#define SYS_WndGetClip      0x110c

SYS_BEGIN_GROUP_GUI(0)
SYS_END_GROUP_GUI(0)

SYS_BEGIN_GROUP_GUI(1)
SYSCALL_BEGIN(handle_t, WndCreate, 12, (handle_t parent, const struct wndattr_t *attribs, unsigned num_attribs))
    PARAM_LONG(parent)
    PARAM_ARRAY(attribs, sizeof(wndattr_t), num_attribs)
    PARAM_LONG(num_attribs)
SYSCALL_END(WndCreate)

SYSCALL_BEGIN(bool, WndClose, 4, (handle_t wnd))
    PARAM_LONG(wnd)
SYSCALL_END(WndClose)

SYSCALL_BEGIN(bool, WndPostMessage, 8, (handle_t wnd, const struct msg_t *msg))
    PARAM_LONG(wnd)
    PARAM_ARRAY(msg, sizeof(msg_t), 1)
SYSCALL_END(WndPostMessage)

SYSCALL_BEGIN(bool, WndInvalidate, 8, (handle_t wnd, const struct MGLrect *rect))
    PARAM_LONG(wnd)
    PARAM_ARRAY(rect, sizeof(MGLrect), 1)
SYSCALL_END(WndInvalidate)

SYSCALL_BEGIN(bool, WndGetMessage, 4, (struct msg_t *msg))
    PARAM_OUT(msg, sizeof(*msg))
SYSCALL_END(WndGetMessage)

SYSCALL_BEGIN(bool, WndGetAttribute, 20, (handle_t wnd, uint32_t id, uint32_t type, void *buf, size_t *size))
    PARAM_LONG(wnd)
    PARAM_LONG(id)
    PARAM_LONG(type)
    PARAM_OUT(buf, *size)
    PARAM_LONG(*size)
    PARAM_OUT(size, sizeof(*size))
SYSCALL_END(WndGetAttribute)

SYSCALL_BEGIN(bool, WndSetAttribute, 20, (handle_t wnd, uint32_t id, uint32_t type, const void *buf, size_t size))
    PARAM_LONG(wnd)
    PARAM_LONG(id)
    PARAM_LONG(type)
    PARAM_ARRAY(buf, sizeof(uint8_t), size)
    PARAM_LONG(size)
SYSCALL_END(WndSetAttribute)

SYSCALL_BEGIN(handle_t, WndOwnRoot, 0, (void))
SYSCALL_END(WndOwnRoot)

SYSCALL_BEGIN(bool, WndQueueInput, 4, (const struct wndinput_t *rec))
    PARAM_ARRAY(rec, sizeof(*rec), 1)
SYSCALL_END(WndQueueInput)

SYSCALL_BEGIN(bool, WndSetFocus, 4, (handle_t wnd))
    PARAM_LONG(wnd)
SYSCALL_END(WndSetFocus)

SYSCALL_BEGIN(bool, WndHasFocus, 4, (handle_t wnd))
    PARAM_LONG(wnd)
SYSCALL_END(WndHasFocus)

SYSCALL_BEGIN(bool, WndSetCapture, 8, (handle_t wnd, bool capture))
    PARAM_LONG(wnd)
    PARAM_LONG(capture)
SYSCALL_END(WndSetCapture)

SYSCALL_BEGIN(bool, WndGetClip, 12, (handle_t wnd, struct MGLrect *rects, size_t *size))
SYSCALL_END(WndGetClip)

SYS_END_GROUP_GUI(1)

/*! @} */
