/* $Id: keyboard.h,v 1.3 2002/04/04 00:08:42 pavlovskii Exp $ */
#ifndef __OS_KEYBOARD_H
#define __OS_KEYBOARD_H

/* "bucky bits"
0x0100000 is reserved for non-ASCII keys, so start with 0x200000 */
#define KBD_BUCKY_RELEASE   0x0100000  /* Key was released */
#define KBD_BUCKY_ALT       0x0200000  /* Alt is pressed */
#define KBD_BUCKY_CTRL      0x0400000  /* Ctrl is pressed */
#define KBD_BUCKY_SHIFT     0x0800000  /* Shift is pressed */
#define KBD_BUCKY_CAPS      0x1000000  /* CapsLock is on */
#define KBD_BUCKY_NUM       0x2000000  /* NumLock is on */
#define KBD_BUCKY_SCRL      0x4000000  /* ScrollLock is on */
#define KBD_BUCKY_ALTGR     0x8000000  /* AltGr is pressed */
#define KBD_BUCKY_ANY   (KBD_BUCKY_ALT | KBD_BUCKY_CTRL | KBD_BUCKY_SHIFT | KBD_BUCKY_ALTGR)

/* "ASCII" values for non-ASCII keys. All of these are user-defined.
function keys: */
#define KEY_F1      0x00010000
#define KEY_F2      (KEY_F1 + 1)
#define KEY_F3      (KEY_F2 + 1)
#define KEY_F4      (KEY_F3 + 1)
#define KEY_F5      (KEY_F4 + 1)
#define KEY_F6      (KEY_F5 + 1)
#define KEY_F7      (KEY_F6 + 1)
#define KEY_F8      (KEY_F7 + 1)
#define KEY_F9      (KEY_F8 + 1)
#define KEY_F10     (KEY_F9 + 1)
#define KEY_F11     (KEY_F10 + 1)
#define KEY_F12     (KEY_F11 + 1)   /* 0x10B */
/* cursor keys */
#define KEY_INS     (KEY_F12 + 1)   /* 0x10C */
#define KEY_DEL     (KEY_INS + 1)
#define KEY_HOME    (KEY_DEL + 1)
#define KEY_END     (KEY_HOME + 1)
#define KEY_PGUP    (KEY_END + 1)
#define KEY_PGDN    (KEY_PGUP + 1)
#define KEY_LEFT    (KEY_PGDN + 1)
#define KEY_UP      (KEY_LEFT + 1)
#define KEY_DOWN    (KEY_UP + 1)
#define KEY_RIGHT   (KEY_DOWN + 1)  /* 0x115 */
/* print screen/sys rq and pause/break */
#define KEY_PRTSC   (KEY_RIGHT + 1) /* 0x116 */
#define KEY_PAUSE   (KEY_PRTSC + 1) /* 0x117 */
/* these return a value but they could also act as additional bucky keys */
#define KEY_LWIN    (KEY_PAUSE + 1) /* 0x118 */
#define KEY_RWIN    (KEY_LWIN + 1)
#define KEY_MENU    (KEY_RWIN + 1)  /* 0x11A */

#endif