/* $Id: common.h,v 1.1 2002/08/17 22:52:14 pavlovskii Exp $ */

#ifndef COMMON_H__
#define COMMON_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifndef __MOBIUS__
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

#ifndef __cplusplus
typedef enum { false = 0, true = 1 } bool;
#endif

int _pwerror(const wchar_t *str);

#define _countof(a) (sizeof(a) / sizeof((a)[0]))

#endif

#pragma pack(push, 1)
typedef struct ed_line_t ed_line_t;
struct ed_line_t
{
    uint16_t length;
    uint16_t length_alloc;
    char text[1];
};
#pragma pack(pop)

enum { evtQuit, evtKeyDown, evtKeyUp };

typedef struct ed_event_t ed_event_t;
struct ed_event_t
{
    unsigned type;
    uint32_t param1;
    uint32_t param2;
};

extern ed_line_t **ed_lines;
extern unsigned ed_num_lines;
extern unsigned ed_scroll_top;
extern unsigned ed_screen_width, ed_screen_height;
extern unsigned ed_row, ed_col;

void EdPostEvent(unsigned type, uint32_t param1, uint32_t param2);
void EdInitEvents(void);
bool EdGetEvent(ed_event_t *evt);

void EdNewFile(void);
bool EdLoadFile(const wchar_t *name);
void EdClearFile(void);

void gotoxy(unsigned col, unsigned row);
void clreol(void);
void clrscr(void);

#ifdef __cplusplus
}
#endif

#endif
