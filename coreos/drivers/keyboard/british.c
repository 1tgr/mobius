#ifndef __KBD_BRITISH_H
#define __KBD_BRITISH_H

#include "keyboard.h"

#define KEY_SYSR    (KBD_BUCKY_ALT | KEY_PRTSC)

keydef_t keys[] =
{
/*  Normal Shift  Ctrl   CtrlS  AltGr  AGrS */
  { 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000 },  /* 0 */
  { 27,    27,    27,    27,    0,     0 },    /* Esc */
  { '1',   '!',   '1',   '!',   0,     0 },    /* 1 */
  { '2',   '"',   '2',   '"',   0,     0 },    /* 2 */
  { '3',   0x00a3,'3',   0x00a3,0,     0 },    /* 3 */
  { '4',   '$',   '4',   '$',   0x20ac,0 },    /* 4 */
  { '5',   '%',   '5',   '%',   0,     0 },    /* 5 */
  { '6',   '^',   '6',   '^',   0,     0 },    /* 6 */
  { '7',   '&',   '7',   '&',   0,     0 },    /* 7 */
  { '8',   '*',   '8',   '*',   0,     0 },    /* 8 */
  { '9',   '(',   '9',   '(',   0,     0 },    /* 9 */
  { '0',   ')',   '0',   ')',   0,     0 },    /* 0 */
  { '-',   '_',   '-',   '_',   0,     0 },    /* - */
  { '=',   '+',   '+',   '+',   0,     0 },    /* = */
  { '\b',  '\b',  '\b',  '\b',  0,     0 },    /* Backspace */
  { '\t',  '\t',  '\t',  '\t',  0,     0 },    /* Tab */
  { 'q',   'Q',   'q',   'Q',   0,     0 },    /* Q */
  { 'w',   'W',   'w',   'W',   0,     0 },    /* W */
  { 'e',   'E',   'e',   'E',   0,     0 },    /* E */
  { 'r',   'R',   'r',   'R',   0,     0 },    /* R */
  { 't',   'T',   't',   'T',   0,     0 },    /* T */
  { 'y',   'Y',   'y',   'Y',   0,     0 },    /* Y */
  { 'u',   'U',   'u',   'U',   0,     0 },    /* U */
  { 'i',   'I',   'i',   'I',   0,     0 },    /* I */
  { 'o',   'O',   'o',   'O',   0,     0 },    /* O */
  { 'p',   'P',   'p',   'P',   0,     0 },    /* P */
  { '[',   '{',   ']',   '}',   0,     0 },    /* [ */
  { ']',   '}',   ']',   '}',   0,     0 },    /* ] */
  { '\n',  '\n',  '\n',  '\n',  0,     0 },    /* Return */
  { 0,     0,     0,     0,     0,     0 },    /* Control */
  { 'a',   'A',   'a',   'A',   0,     0 },    /* A */
  { 's',   'S',   's',   'S',   0,     0 },    /* S */
  { 'd',   'D',   'd',   'D',   0,     0 },    /* D */
  { 'f',   'F',   'f',   'F',   0,     0 },    /* F */
  { 'g',   'G',   'g',   'G',   0,     0 },    /* G */
  { 'h',   'H',   'h',   'H',   0,     0 },    /* H */
  { 'j',   'J',   'j',   'J',   0,     0 },    /* J */
  { 'k',   'K',   'k',   'K',   0,     0 },    /* K */
  { 'l',   'L',   'l',   'L',   0,     0 },    /* L */
  { ';',   ':',   ';',   ':',   0,     0 },    /* ; */
  { '\'',  '@',   '\'',  '@',   0,     0 },    /* ' */
  { '`',   '¬',   '`',   '¬',   '¦',   0 },    /* ` */
  { 0,     0,     0,     0,     0,     0 },    /* Left Shift */
  { '#',   '~',   '#',   '~',   0,     0 },    /* # */
  { 'z',   'Z',   'z',   'Z',   0,     0 },    /* Z */
  { 'x',   'X',   'x',   'X',   0,     0 },    /* X */
  { 'c',   'C',   'c',   'C',   0,     0 },    /* C */
  { 'v',   'V',   'v',   'V',   0,     0 },    /* V */
  { 'b',   'B',   'b',   'B',   0,     0 },    /* B */
  { 'n',   'N',   'n',   'N',   0,     0 },    /* N */
  { 'm',   'M',   'm',   'M',   0,     0 },    /* M */
  { ',',   '<',   ',',   '<',   0,     0 },    /* , */
  { '.',   '>',   '.',   '>',   0,     0 },    /* . */
  { '/',   '?',   '/',   '?',   0,     0 },    /* / */
  { 0,     0,     0,     0,     0,     0 },    /* Right Shift */
  { KEY_PRTSC, 0, 0, 0, 0, 0,            },    /* Print Screen */
  { 0,     0,     0,     0,     0,     0 },    /* Left Alt */
  { ' ',   ' ',   ' ',   ' ',   0,     0 },    /* Space */
  { 0,     0,     0,     0,     0,     0 },    /* Caps Lock */
  { KEY_F1,KEY_F1,KEY_F1,KEY_F1,0,     0 },    /* F1 */
  { KEY_F2,KEY_F2,KEY_F2,KEY_F2,0,     0 },    /* F2 */
  { KEY_F3,KEY_F3,KEY_F3,KEY_F3,0,     0 },    /* F3 */
  { KEY_F4,KEY_F4,KEY_F4,KEY_F4,0,     0 },    /* F4 */
  { KEY_F5,KEY_F5,KEY_F5,KEY_F5,0,     0 },    /* F5 */
  { KEY_F6,KEY_F6,KEY_F6,KEY_F6,0,     0 },    /* F6 */
  { KEY_F7,KEY_F7,KEY_F7,KEY_F7,0,     0 },    /* F7 */
  { KEY_F8,KEY_F8,KEY_F8,KEY_F8,0,     0 },    /* F8 */
  { KEY_F9,KEY_F9,KEY_F9,KEY_F9,0,     0 },    /* F9 */
  { KEY_F10, KEY_F10, KEY_F10, KEY_F10, 0, 0 }, /* F10 */
  { 0,     0,     0,     0,     0,     0,    }, /* Num Lock */
  { 0,     0,     0,     0,     0,     0,    }, /* Scroll Lock */
  { KEY_HOME,KEY_HOME,KEY_HOME,KEY_HOME,0, 0 }, /* Home */
  { KEY_UP,  KEY_UP,  KEY_UP,  KEY_UP,  0, 0 }, /* Up */
  { KEY_PGUP,KEY_PGUP,KEY_PGUP,KEY_PGUP,0, 0 }, /* Page Up */
  { '-',     '-',     0x2013,  0x2013,  0, 0 }, /* Num - */
  { KEY_LEFT,KEY_LEFT,KEY_LEFT,KEY_LEFT,0, 0 }, /* Left */
  { 0,       0,       0,       0,       0, 0 }, /* Num 5 */
  { KEY_RIGHT,KEY_RIGHT,KEY_RIGHT,KEY_RIGHT,0, 0 }, /* Right*/
  { '+',     '+',     '+',     '+',     0, 0 }, /* Num + */
  { KEY_END, KEY_END, KEY_END, KEY_END, 0, 0 }, /* End */
  { KEY_DOWN,KEY_DOWN,KEY_DOWN,KEY_DOWN,0, 0 }, /* Down */
  { KEY_PGDN,KEY_PGDN,KEY_PGDN,KEY_PGDN,0, 0 }, /* Page Down */
  { KEY_INS, KEY_INS, KEY_INS, KEY_INS, 0, 0 }, /* Insert */
  { KEY_DEL, KEY_DEL, KEY_DEL, KEY_DEL, 0, 0 }, /* Delete */
  { KEY_SYSR,KEY_SYSR,KEY_SYSR,KEY_SYSR,0, 0 }, /* Sys Req */
  { 0,       0,       0,       0,       0, 0 }, /* Scancode 55 */
  { '\\',    '|',    '\\',    '|',      0, 0 }, /* Scancode 56 */
  { KEY_F11, KEY_F11, KEY_F11, KEY_F11, 0, 0 }, /* F11 */
  { KEY_F12, KEY_F12, KEY_F12, KEY_F12, 0, 0 }, /* F12 */
  { 0,       0,       0,       0,       0, 0 }, /* Scancode 59 */
  { 0,       0,       0,       0,       0, 0 }, /* Scancode 5A */
  { KEY_LWIN,KEY_LWIN,KEY_LWIN,KEY_LWIN,0, 0 }, /* Left Windows */
  { KEY_RWIN,KEY_RWIN,KEY_RWIN,KEY_RWIN,0, 0 }, /* Right Windows */
  { KEY_MENU,KEY_MENU,KEY_MENU,KEY_MENU,0, 0 }, /* Context Menu */
};

#endif
