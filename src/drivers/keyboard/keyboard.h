/* $Id: keyboard.h,v 1.3 2002/04/10 12:24:11 pavlovskii Exp $ */

#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include <os/keyboard.h>

#define KEYB_PORT               0x60    /* keyboard port */
#define KEYB_CTRL               0x64    /* keyboard controller port */

#define KCTRL_ENABLE_AUX		0xA8	/* enable aux port (PS/2 mouse) */
#define KCTRL_WRITE_CMD_BYTE    0x60    /* write to command register */
#define KCTRL_WRITE_AUX			0xD4	/* write next byte at port 60 to aux port */

/* flags for KCTRL_WRITE_CMD_BYTE */
#define KCTRL_IRQ1              0x01
#define KCTRL_IRQ12             0x02
#define KCTRL_SYS               0x04
#define KCTRL_OVERRIDE_INHIBIT  0x08
#define KCTRL_DISABLE_KEYB      0x10
#define KCTRL_DISABLE_AUX       0x20
#define KCTRL_TRANSLATE_XT      0x40

/* commands to keyboard */
#define KEYB_SET_LEDS			0xED
#define KEYB_SET_SCANCODE_SET   0xF0
#define KEYB_IDENTIFY           0xF2
#define KEYB_SET_TYPEMATIC      0xF3
#define KEYB_ENABLE             0xF4
#define KEYB_RESET_DISABLE      0xF5
#define KEYB_ALL_TYPM_MAKE_BRK  0xFA

/* default ACK from keyboard following command */
#define KEYB_ACK                "\xFA"

/* commands to aux device (PS/2 mouse) */
#define AUX_INFORMATION			0xE9
#define AUX_ENABLE				0xF4
#define AUX_IDENTIFY			0xF2
#define AUX_RESET				0xFF

/* "raw" set 1 scancodes from PC keyboard */
#define RAW_LEFT_CTRL   0x1D
#define RAW_LEFT_SHIFT  0x2A
#define RAW_CAPS_LOCK   0x3A
#define RAW_LEFT_ALT    0x38
#define RAW_RIGHT_ALT   0x6038
#define RAW_RIGHT_CTRL  0x601D
#define RAW_RIGHT_SHIFT 0x36
#define RAW_SCROLL_LOCK 0x46
#define RAW_NUM_LOCK    0x45
#define RAW_NUM7		0x47
#define RAW_NUM8		0x48
#define RAW_NUM9		0x49
#define RAW_NUM4		0x4b
#define RAW_NUM5		0x4c
#define RAW_NUM6		0x4d
#define RAW_NUM1		0x4f
#define RAW_NUM2		0x50
#define RAW_NUM3		0x51
#define RAW_NUM0		0x52

#endif