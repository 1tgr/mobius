/* $Id: sysdef.h,v 1.1 2002/09/13 23:13:02 pavlovskii Exp $ */

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
SYSCALL_GUI(handle_t, WndCreate, 12, (handle_t, const struct wndattr_t *, unsigned))
SYSCALL_GUI(bool, WndClose, 4, (handle_t))
SYSCALL_GUI(bool, WndPostMessage, 8, (handle_t, const struct msg_t *))
SYSCALL_GUI(bool, WndInvalidate, 8, (handle_t, const struct MGLrect *))
SYSCALL_GUI(bool, WndGetMessage, 4, (struct msg_t*))
SYSCALL_GUI(bool, WndGetAttribute, 20, (handle_t, uint32_t, uint32_t, void *, size_t*))
SYSCALL_GUI(bool, WndSetAttribute, 20, (handle_t, uint32_t, uint32_t, const void *, size_t))
SYSCALL_GUI(handle_t, WndOwnRoot, 0, (void))
SYSCALL_GUI(bool, WndQueueInput, 4, (const struct wndinput_t*))
SYSCALL_GUI(bool, WndSetFocus, 4, (handle_t))
SYSCALL_GUI(bool, WndHasFocus, 4, (handle_t))
SYSCALL_GUI(bool, WndSetCapture, 8, (handle_t, bool))
SYSCALL_GUI(bool, WndGetClip, 12, (handle_t, struct MGLrect *, size_t*))
SYS_END_GROUP_GUI(1)

/*! @} */
