/* $Id: gui.h,v 1.2 2002/04/20 12:34:38 pavlovskii Exp $ */

#ifndef __OS_GUI_H
#define __OS_GUI_H

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 *  \ingroup    libsys
 *  \defgroup   gui Graphical User Interface
 *  @{
 */

#include <gl/types.h>

typedef struct wndattr_t wndattr_t;
struct wndattr_t
{
    uint32_t id;
    uint32_t type;
    size_t size;

    union
    {
        uint8_t ui08;
        uint16_t ui16;
        uint32_t ui32;
        addr_t addr;
        MGLrect *rect;
        void *ptr;
        uint8_t *data;
        wchar_t *str;
    } data;
};

#define WND_TYPE_UINT8      '\1u08'
#define WND_TYPE_UINT16     '\1u16'
#define WND_TYPE_UINT32     '\1u32'
#define WND_TYPE_RECT       'rect'
#define WND_TYPE_STRING     'wstr'
#define WND_TYPE_DATA       'data'
#define WND_TYPE_VOID       '\1ptr'

/*
 * Window attributes over 0x80000000 (i.e. last char > 0x80) are dynamically 
 *  generated.
 */
#define WND_ATTR_POSITION   1
#define WND_ATTR_TITLE      2
#define WND_ATTR_USERDATA   3

#define WND_TYPE_IS_DIRECT(type)    ((((type) >> 24) & 0xff) == 1)

typedef struct msg_t msg_t;
struct msg_t
{
    handle_t window;
    uint32_t code;
    union
    {
        uint32_t params[3];

        uint32_t key;

        struct
        {
            uint32_t buttons;
            MGLreal x;
            MGLreal y;
        } mouse;

        struct
        {
            int delta;
            MGLreal x;
            MGLreal y;
        } wheel;
    } p;
};

#define MSG_QUIT            1
#define MSG_PAINT           2
#define MSG_KEYDOWN         3
#define MSG_KEYUP           4
#define MSG_FOCUS           5
#define MSG_BLUR            6
#define MSG_MOUSEDOWN       7
#define MSG_MOUSEUP         8
#define MSG_MOUSEMOVE       9
#define MSG_MOUSEWHEEL      10
#define MSG_CLOSE           11

typedef struct wndinput_t wndinput_t;
struct wndinput_t
{
    uint32_t type;

    union
    {
        uint32_t key;

        struct
        {
            MGLreal x;
            MGLreal y;
        } mouse_move;

        struct
        {
            MGLreal x;
            MGLreal y;
            uint32_t buttons;
        } mouse_down, mouse_up;

        struct
        {
            MGLreal x;
            MGLreal y;
            int dwheel;
        } mouse_wheel;
    } data;
};

#define WND_INPUT_KEY_DOWN      0
#define WND_INPUT_KEY_UP        1
#define WND_INPUT_MOUSE_MOVE    2
#define WND_INPUT_MOUSE_DOWN    3
#define WND_INPUT_MOUSE_UP      4
#define WND_INPUT_MOUSE_WHEEL   5

/*! @} */

#ifdef __cplusplus
}
#endif

#endif