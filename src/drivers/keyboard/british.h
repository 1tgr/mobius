#ifndef __KBD_BRITISH_H
#define __KBD_BRITISH_H

static const uint32_t keys[] =
{
	0,
/*  Esc    1    2    3    4    5    6    7    8    9    0    -    =   Bksp */
	27,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
/*  Tab    q    w    e    r    t    y    u    i    o    p    [    ]        */
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
/*  Ctrl   a    s    d    f    g    h    j    k    l    ;    '    `        */
	0,
		  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`',
/*  LShift #    z    x    c    v    b    n    m    ,    .    /   RShift    */
	0,
		  '#', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,

/*  PrtScr     LAlt      Space,    Caps                                    */
	KEY_PRTSC, 0,        ' ',      0, 

	KEY_F1,    KEY_F2,    KEY_F3,   KEY_F4,    KEY_F5, 
	KEY_F6,    KEY_F7,    KEY_F8,   KEY_F9,    KEY_F10, 
	0,/*Num*/  0,/*Scrl*/ KEY_HOME, KEY_UP,    KEY_PGUP, 
	'-',       KEY_LEFT,  0,/*Pad5*/KEY_RIGHT, '+',
	KEY_END,   KEY_DOWN,  KEY_PGDN, KEY_INS,   KEY_DEL,
	KBD_BUCKY_ALT | KEY_PRTSC, 
			   0,/*55*/   '\\',/*56*/  KEY_F11,   KEY_F12,
	0,/*59*/   0,/*5a*/   KEY_LWIN,    KEY_RWIN,  KEY_MENU
};

static const uint32_t keys_shift[] =
{
	0,
/*  Esc    1    2    3    4    5    6    7    8    9    0    -    =   Bksp */
	27,   '!', '\"',0xa3, '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
/*  Tab    q    w    e    r    t    y    u    i    o    p    [    ]        */
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
/*  Ctrl   a    s    d    f    g    h    j    k    l    ;    '    `        */
	0,
		  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '@', 0xac,
/*  LShift #    z    x    c    v    b    n    m    ,    .    /   RShift    */
	0,
		  '~', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,

/*  PrtScr     LAlt      Space,    Caps                                    */
	KEY_PRTSC, 0,        ' ',      0, 

	KEY_F1,    KEY_F2,    KEY_F3,   KEY_F4,    KEY_F5, 
	KEY_F6,    KEY_F7,    KEY_F8,   KEY_F9,    KEY_F10, 
	0,/*Num*/  0,/*Scrl*/ KEY_HOME, KEY_UP,    KEY_PGUP, 
	'-',       KEY_LEFT,  0,/*Pad5*/KEY_RIGHT, '+',
	KEY_END,   KEY_DOWN,  KEY_PGDN, KEY_INS,   KEY_DEL,
	KBD_BUCKY_ALT | KEY_PRTSC, 
			   '!',/*55*/ '|',/*56*/KEY_F11,   KEY_F12,
	0,/*59*/   0,/*5a*/   KEY_LWIN, KEY_RWIN,  KEY_MENU
};

#endif