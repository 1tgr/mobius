/* $Id: main.c,v 1.1 2002/08/17 22:52:14 pavlovskii Exp $ */

#include "common.h"
#include <os/keyboard.h>
#include <stddef.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <locale.h>
#endif

ed_line_t **ed_lines;
unsigned ed_num_lines;
unsigned ed_scroll_top;
unsigned ed_screen_width = 80, ed_screen_height = 25;
unsigned ed_row, ed_col;
unsigned ed_buffer_x, ed_buffer_y, ed_buffer_width, ed_buffer_height;

const wchar_t ed_linedraw_w[] =
{
    0x250c, /* top left */
    0x2510, /* top right */
    0x2514, /* bottom left */
    0x2518, /* bottom right */
    0x2502, /* vertical */
    0x2500, /* horizontal */
};

char ed_linedraw[_countof(ed_linedraw_w)][4];

void EdRenderLines(unsigned top, unsigned bottom)
{
    unsigned line;
    char *buf;
    size_t size;

    if (bottom > ed_num_lines)
        bottom = ed_num_lines;
    if (bottom <= ed_scroll_top || top > ed_scroll_top + ed_screen_height)
        return;

    buf = malloc(ed_buffer_width + 1);
    for (line = top; line < bottom; line++)
    {
        clreol();
        size = min(ed_buffer_width, ed_lines[line]->length);
        memcpy(buf, ed_lines[line]->text, size + 1);
        buf[size] = '\0';
        gotoxy(ed_buffer_x, ed_buffer_y + line - ed_scroll_top);
        fputs(buf, stdout);
    }

    free(buf);
}

unsigned EdConstrainCursor(void)
{
    if (ed_row == ed_num_lines)
        return 0;
    else if (ed_col > ed_lines[ed_row]->length)
        return ed_lines[ed_row]->length;
    else
        return ed_col;
}

void EdUpdateCursor(void)
{
    unsigned col;
    bool redraw;

    redraw = false;
    if (ed_row < ed_scroll_top)
    {
        ed_scroll_top = ed_row;
        redraw = true;
    }
    else if (ed_row > ed_scroll_top + ed_buffer_height - 1)
    {
        ed_scroll_top = ed_row - ed_buffer_height + 1;
        redraw = true;
    }

    if (redraw)
        EdRenderLines(ed_scroll_top, ed_scroll_top + ed_buffer_height);

    col = EdConstrainCursor();
    gotoxy(ed_buffer_x + col, ed_buffer_y + ed_row - ed_scroll_top);
}

void EdHandleKeyDown(uint32_t key)
{
    ed_line_t *line;
    char mb[4];

    switch (key)
    {
    case 0x1b:
        EdPostEvent(evtQuit, 0, 0);
        break;

    case KEY_RIGHT:
        if (ed_row < ed_num_lines &&            /* check for cursor on dummy line */
            ed_col < ed_lines[ed_row]->length)
        {
            ed_col++;
            EdUpdateCursor();
            break;
        }
        else
        {
            ed_col = 0;
            /* fall through */
        }

    case KEY_DOWN:
        if (ed_row < ed_num_lines)
        {
            ed_row++;
            EdUpdateCursor();
        }
        break;

    case KEY_LEFT:
        if (ed_row < ed_num_lines &&
            ed_col > 0)
        {
            ed_col--;
            EdUpdateCursor();
            break;
        }
        else
        {
            if (ed_row > 0)
                ed_col = ed_lines[ed_row - 1]->length;

            /* fall through */
        }

    case KEY_UP:
        if (ed_row > 0)
        {
            ed_row--;
            EdUpdateCursor();
        }
        break;

    case KEY_HOME:
        ed_col = 0;
        EdUpdateCursor();
        break;

    case KEY_END:
        if (ed_row < ed_num_lines)
            ed_col = ed_lines[ed_row]->length;
        else
            ed_col = 0;
        EdUpdateCursor();
        break;

    case '\b':
        ed_col = EdConstrainCursor();
        /*
         * If backspacing start of line...
         *  (note: ed_col == 0 for dummy last line)
         */
        if (ed_col == 0 && key == '\b')
        {
            /* Can't backspace beyond the start of the file */
            if (ed_row == 0)
                break;

            /* Go up a row... */
            ed_row--;
            /* ...to the end of that line */
            ed_col = ed_lines[ed_row]->length;
            /* append current line onto previous line */
        }
        else
        {
            line = ed_lines[ed_row];
            memmove(line->text + ed_col - 1,
                line->text + ed_col,
                line->length - ed_col + 1); /* add 1 to move NUL terminator */

            ed_col--;

            line->length--;
        }

        EdRenderLines(ed_row, ed_row + 1);
        EdUpdateCursor();
        break;

    case KEY_DEL:
        ed_col = EdConstrainCursor();
        /*
         * If deleting at end of line...
         *  (note: ed_col == 0 for dummy last line so this is OK)
         */
        if (ed_col == ed_lines[ed_row]->length)
        {
            /* Can't delete beyond the end of the file */
            if (ed_row == ed_num_lines - 1)
                break;

            /* append next line onto current line */
        }
        else
        {
            line = ed_lines[ed_row];
            memmove(line->text + ed_col,
                line->text + ed_col + 1,
                line->length - ed_col + 1); /* add 1 to move NUL terminator */

            line->length--;
        }

        EdRenderLines(ed_row, ed_row + 1);
        EdUpdateCursor();
        break;

    default:
        key &= ~KBD_BUCKY_ANY;
        if (key < 0x10000 && key != 0)
        {
            if (ed_row == ed_num_lines)
            {
                ed_lines = realloc(ed_lines, sizeof(ed_line_t*) * (ed_num_lines + 1));
                ed_lines[ed_row] = malloc(sizeof(ed_line_t) + 15);
                ed_lines[ed_row]->length = 0;
                ed_lines[ed_row]->length_alloc = 16;
                ed_lines[ed_row]->text[0] = '\0';
                ed_num_lines++;
            }

            mb[0] = mb[1] = mb[2] = mb[3] = 0;
            wctomb(mb, (wchar_t) key);
            line = ed_lines[ed_row];
            if (line->length + strlen(mb) >= (size_t) line->length_alloc - 1)
            {
                line->length_alloc += max(strlen(mb), 16);
                line = ed_lines[ed_row] = 
                    realloc(line, sizeof(ed_line_t*) - 1 + line->length_alloc);
            }

            ed_col = EdConstrainCursor();
            memmove(line->text + ed_col + 1, 
                line->text + ed_col,
                line->length - ed_col + 1); /* add 1 to move NUL terminator */
            memcpy(line->text + ed_col, mb, strlen(mb));
            line->length++;
            ed_col++;
            EdRenderLines(ed_row, ed_row + 1);
            EdUpdateCursor();
        }
        break;
    }
}

void EdDispatchEvent(const ed_event_t *evt)
{
    switch (evt->type)
    {
    case evtKeyDown:
        EdHandleKeyDown(evt->param1);
        break;
    }
}

int wmain(int argc, wchar_t **argv)
{
    ed_event_t evt;
    unsigned i;
    wchar_t wc[2];

#ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | 
		_CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
    setlocale(LC_ALL, "");
#endif

    wc[1] = '\0';
    for (i = 0; i < _countof(ed_linedraw); i++)
    {
        wc[0] = ed_linedraw_w[i];
        wcstombs(ed_linedraw[i], wc, _countof(ed_linedraw[i]));
    }

    clrscr();

    if (argc == 1)
        EdNewFile();
    else if (!EdLoadFile(argv[1]))
        return EXIT_FAILURE;

    ed_buffer_x = ed_buffer_y = 1;
    ed_buffer_width = ed_screen_width - 2;
    ed_buffer_height = ed_screen_height - 2;

    gotoxy(0, 0);
    fputs(ed_linedraw[0], stdout);
    for (i = 1; i < ed_screen_width - 1; i++)
        fputs(ed_linedraw[5], stdout);
    fputs(ed_linedraw[1], stdout);
    for (i = 1; i < ed_screen_height - 1; i++)
    {
        gotoxy(0, i);
        fputs(ed_linedraw[4], stdout);
        gotoxy(ed_screen_width - 1, i);
        fputs(ed_linedraw[4], stdout);
    }
    gotoxy(0, ed_screen_height - 1);
    fputs(ed_linedraw[2], stdout);
    for (i = 1; i < ed_screen_width - 1; i++)
        fputs(ed_linedraw[5], stdout);
    fputs(ed_linedraw[3], stdout);

    EdRenderLines(0, ed_buffer_height);
    EdUpdateCursor();

    EdInitEvents();
    while (EdGetEvent(&evt))
        EdDispatchEvent(&evt);

    EdClearFile();
    return EXIT_SUCCESS;
}
