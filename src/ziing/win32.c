/* $Id: win32.c,v 1.1 2002/08/17 22:52:14 pavlovskii Exp $ */

#include "common.h"
#include <conio.h>
#include <os/keyboard.h>
#include <windows.h>
#include <locale.h>

static bool ed_quit;

typedef struct keydef_t keydef_t;
struct keydef_t
{
    uint32_t normal;
    uint32_t shift;
    uint32_t control;
    uint32_t control_shift;
    uint32_t altgr;
    uint32_t altgr_shift;
};

#define KEY_SYSR    (KBD_BUCKY_ALT | KEY_PRTSC)

static keydef_t keys[] =
{
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

static COORD c;

void gotoxy(unsigned col, unsigned row)
{
    c.X = col;
    c.Y = row;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void clreol(void)
{
    HANDLE h;
    CONSOLE_SCREEN_BUFFER_INFO info;
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(h, &info);
    FillConsoleOutputCharacter(h, 
        ' ', 
        ed_screen_width - info.dwCursorPosition.X,
        info.dwCursorPosition,
        NULL);
}

void clrscr(void)
{
    c.X = c.Y = 0;
    FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), 
        ' ', 
        ed_screen_width * ed_screen_height,
        c,
        NULL);
    gotoxy(0, 0);
}

int _pwerror(const wchar_t *str)
{
    wprintf(L"%s: %S\n", str, strerror(errno));
    return 0;
}

bool EdGetEvent(ed_event_t *evt)
{
    HANDLE h;
    DWORD key;
    INPUT_RECORD input;
    DWORD num_read;

    if (ed_quit)
        return false;

    h = GetStdHandle(STD_INPUT_HANDLE);
    while (true)
    {
        if (!ReadConsoleInputW(h, &input, 1, &num_read) ||
            num_read == 0)
            return false;

        if (input.EventType != KEY_EVENT)
            continue;

        evt->type = input.Event.KeyEvent.bKeyDown ? evtKeyDown : evtKeyUp;
        key = input.Event.KeyEvent.uChar.UnicodeChar;

        if (key == 0)
        {
            key = input.Event.KeyEvent.wVirtualScanCode;
            if (key >= 0x3b && key < sizeof(keys) / sizeof(keys[0]) + 0x3b)
                evt->param1 = keys[key - 0x3b].normal;
            else
                continue;
        }
        else
            evt->param1 = input.Event.KeyEvent.uChar.UnicodeChar;

        evt->param2 = 0;
        break;
    }

    return true;
}

void EdPostEvent(unsigned type, uint32_t param1, uint32_t param2)
{
    if (type == evtQuit)
        ed_quit = true;
}

void EdInitEvents(void)
{
    /*CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    ed_screen_width = info.dwSize.X;
    ed_screen_height = info.dwSize.Y - 1;*/
}
