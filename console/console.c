/* $Id: console.c,v 1.2 2003/06/05 22:02:12 pavlovskii Exp $ */

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include <os/port.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <os/ioctl.h>
#include <os/keyboard.h>
#include <os/video.h>

typedef struct console_t console_t;
struct console_t
{
    handle_t client;
    unsigned width, height;
    unsigned x, y;
    bool cursor_visible;
    unsigned buffer;
    rect_t rect;
    wchar_t *buf_chars;
    uint8_t *buf_attribs;

    uint8_t attribs;
    uint8_t default_attribs;
    unsigned save_x, save_y;
    unsigned escape, esc[3], esc_param;
} consoles[10];

unsigned num_consoles;
console_t *current;
videomode_t mode;
uint8_t *mem, *buffer_mem, *video_mem;
handle_t mux_draw;

unsigned char_width = 8, char_height = 8, font_height = 8;

extern uint8_t font8x8[];

static const uint32_t colours[16] =
{
    0x000000,
    0x800000,
    0x008000,
    0x808000,
    0x000080,
    0x800080,
    0x008080,
    0xC0C0C0,
    0x808080,
    0xFF0000,
    0x00FF00,
    0xFFFF00,
    0x0000FF,
    0xFF00FF,
    0x00FFFF,
    0xFFFFFF,
};

uint8_t ConRgbToIndex(uint32_t rgb)
{
    uint8_t r, g, b;
    r = COLOUR_RED(rgb);
    g = COLOUR_GREEN(rgb);
    b = COLOUR_BLUE(rgb);
    r = (r * 6) / 256;
    g = (g * 6) / 256;
    b = (b * 6) / 256;
    return 40 + r * 36 + g * 6 + b;
}

uint8_t ConGetBackgroundIndex(console_t *console)
{
    return ConRgbToIndex(colours[((console->attribs >> 4) & 0x0f)]);
}

uint8_t ConGetForegroundIndex(console_t *console)
{
    return ConRgbToIndex(colours[(console->attribs & 0x0f)]);
}

void ConDrawCursor(console_t *console, bool do_draw)
{
    unsigned x, y;
    uint8_t *line;

    if (console == current &&
        do_draw != console->cursor_visible)
    {
        line = (video_mem + 
            (console->rect.top + (console->y + 1) * char_height - 2) 
                * mode.bytesPerLine) + 
            console->rect.left + console->x * char_width;

        for (y = char_height - 2; y < char_height; y++)
        {
            for (x = 0; x < char_width; x++)
                line[x] ^= 7;
            line += mode.bytesPerLine;
        }

        current->cursor_visible = do_draw;
    }
}


size_t wcsto437(char *mbstr, const wchar_t *wcstr, size_t count);

void ConWriteChar(unsigned x, unsigned y, wchar_t ch, uint8_t fg, uint8_t bg)
{
    unsigned ax, ay;
    uint8_t *line, mask, *data;
    char mb[2];
    
    wcsto437(mb, &ch, 1);
    data = font8x8 + font_height * (uint8_t) mb[0];
    fg = ConRgbToIndex(colours[fg & 0x0f]);
    bg = ConRgbToIndex(colours[bg & 0x0f]);
    for (ay = 0; ay < char_height; ay++)
    {
        line = (mem + (y + ay) * mode.bytesPerLine) + x;
        ax = char_width;
        for (mask = 1; ax > 0; mask <<= 1)
        {
            ax--;
            if ((data[ay] & mask) != 0)
                line[ax] = fg;
            else
                line[ax] = bg;
        }
    }
}

void ConRedraw(console_t *console)
{
    unsigned x, y, i;

    if (console->buffer != current->buffer)
        return;

    i = 0;
    for (x = 0; x < console->height; y++)
        for (y = 0; y < console->width; y++)
        {
            ConWriteChar(console->rect.left + x * char_width,
                console->rect.top + y * char_height,
                console->buf_chars[i],
                console->buf_attribs[i] & 0x0f,
                (console->buf_attribs[i] >> 4) & 0x0f);
            i++;
        }
}

void ConKeyboardThread(void)
{
    uint32_t ch, code;
    handle_t keyboard;
    unsigned old_buffer, i;

    keyboard = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
    while (FsReadSync(keyboard, &ch, sizeof(ch), NULL))
    {
        code = ch & ~KBD_BUCKY_ANY;
        if ((ch & KBD_BUCKY_ALT) != 0 &&
            code >= KEY_F1 && 
            code <= KEY_F12)
        {
            MuxAcquire(mux_draw);
            ConDrawCursor(current, false);

            old_buffer = current->buffer;
            current = consoles + code - KEY_F1;

            if (old_buffer != current->buffer)
                for (i = 0; i < num_consoles; i++)
                    if (consoles[i].buffer == current->buffer)
                        ConRedraw(consoles + i);

            ConDrawCursor(current, true);
            MuxRelease(mux_draw);
        }
        else if (ch == (KBD_BUCKY_CTRL | KBD_BUCKY_ALT | KEY_DEL))
            SysShutdown(SHUTDOWN_REBOOT);
        else if (ch == (KBD_BUCKY_ALT | '\t') || 
            ch == (KBD_BUCKY_ALT | KBD_BUCKY_SHIFT | '\t'))
        {
            MuxAcquire(mux_draw);
            ConDrawCursor(current, false);

            if (ch & KBD_BUCKY_SHIFT)
            {
                if (current - consoles - 1 < 0)
                    current = consoles + num_consoles - 1;
                else
                    current--;
            }
            else
            {
                if (current - consoles + 1 >= num_consoles)
                    current = consoles;
                else
                    current++;
            }

            ConDrawCursor(current, true);
            MuxRelease(mux_draw);
        }
        else if (ch == KEY_F5)
        {
            MuxAcquire(mux_draw);
            for (i = 0; i < num_consoles; i++)
                if (consoles[i].buffer == current->buffer &&
                    consoles + i != current)
                    consoles[i].buffer = -1;
            current->rect.left = current->rect.top = 0;
            current->rect.right = mode.width;
            current->rect.bottom = mode.height;
            current->width = (current->rect.right - current->rect.left) / char_width;
            current->height = (current->rect.bottom - current->rect.top) / char_height;
            ConRedraw(current);
            MuxRelease(mux_draw);
        }
        else if (current != NULL && current->client != NULL)
            FsWriteSync(current->client, &ch, sizeof(ch), NULL);
    }
}

void ConCursorThread(void)
{
    while (true)
    {
        if (current != NULL)
        {
            MuxAcquire(mux_draw);
            ConDrawCursor(current, !current->cursor_visible);
            MuxRelease(mux_draw);
        }

        ThrSleep(500);
    }
}

void ConScroll(console_t *console)
{
    unsigned y;
    uint8_t *src, *dest;

    while (console->y >= console->height)
    {
        if (console->buffer == current->buffer)
        {
            dest = mem + console->rect.top * mode.bytesPerLine + console->rect.left;
            src = dest + font_height * mode.bytesPerLine;
            for (y = console->rect.top; y < console->rect.bottom - font_height; y++)
            {
                memmove(dest, src, console->rect.right - console->rect.left);
                dest += mode.bytesPerLine;
                src += mode.bytesPerLine;
            }

            for (; y < console->rect.bottom; y++)
            {
                memset(dest, ConGetBackgroundIndex(console),
                    console->rect.right - console->rect.left);
                dest += mode.bytesPerLine;
            }
        }

        console->y--;
    }
}

void ConUpdateCursor(console_t *console)
{
}

void ConClear(console_t *console)
{
    unsigned y;
    uint8_t *line;

    if (console->buffer == current->buffer)
    {
        line = mem + console->rect.top * mode.bytesPerLine;
        for (y = console->rect.top; y < console->rect.bottom; y++)
        {
            memset(line,
                ConGetBackgroundIndex(console),
                console->rect.right - console->rect.left);

            line += mode.bytesPerLine;
        }
    }

    console->x = console->y = 0;
}

void ConClearEol(console_t *console)
{
    unsigned y, width;
    uint8_t *line;

    if (console->buffer == current->buffer)
    {
        line = (mem + (console->rect.top + console->y * char_height) * mode.bytesPerLine) + 
            console->rect.left + console->x * char_width;
        width = console->rect.right - console->rect.left - console->x * char_width;
        for (y = 0; y < char_height; y++)
        {
            memset(line, ConGetBackgroundIndex(console), width);
            line += mode.bytesPerLine;
        }
    }
}

void ConBlitToScreen(const rect_t *rect)
{
    unsigned offset, y;

    offset = rect->top * mode.bytesPerLine + rect->left;
    for (y = rect->top; y < rect->bottom; y++)
    {
        memcpy(video_mem + offset, buffer_mem + offset, 
            rect->right - rect->left);
        offset += mode.bytesPerLine;
    }
}

void ConDoEscape(console_t *console, int code, unsigned num, const unsigned *esc)
{
    unsigned i;
    static const char ansi_to_vga[] =
    {
        0, 4, 2, 6, 1, 5, 3, 7
    };

    switch (code)
    {
    case 'H':    /* ESC[PL;PcH - cursor position */
    case 'f':    /* ESC[PL;Pcf - cursor position */
        if (num > 0)
            console->y = min(esc[0], console->height - 1);
        if (num > 1)
            console->x = min(esc[1], console->width - 1);
        ConUpdateCursor(console);
        break;
    
    case 'A':    /* ESC[PnA - cursor up */
        if (num > 0 && console->y >= esc[0])
        {
            console->y -= esc[0];
            ConUpdateCursor(console);
        }
        break;
    
    case 'B':    /* ESC[PnB - cursor down */
        if (num > 0 && console->y < console->height + esc[0])
        {
            console->y += esc[0];
            ConUpdateCursor(console);
        }
        break;

    case 'C':    /* ESC[PnC - cursor forward */
        if (num > 0 && console->x < console->width + esc[0])
        {
            console->x += esc[0];
            ConUpdateCursor(console);
        }
        break;
    
    case 'D':    /* ESC[PnD - cursor backward */
        if (num > 0 && console->x >= esc[0])
        {
            console->x -= esc[0];
            ConUpdateCursor(console);
        }
        break;

    case 's':    /* ESC[s - save cursor position */
        console->save_x = console->x;
        console->save_y = console->y;
        break;

    case 'u':    /* ESC[u - restore cursor position */
        console->x = console->save_x;
        console->y = console->save_y;
        ConUpdateCursor(console);
        break;

    case 'J':    /* ESC[2J - clear screen */
        ConClear(console);
        break;

    case 'K':    /* ESC[K - erase line */
	ConClearEol(console);
        break;

    case 'm':    /* ESC[Ps;...Psm - set graphics mode */
        if (num == 0)
            console->attribs = console->default_attribs;
        else for (i = 0; i < num; i++)
        {
            if (esc[i] <= 8)
            {
                switch (esc[i])
                {
                case 0:    /* all attributes off */
                    console->attribs &= 0x77;
                    break;
                case 1:    /* bold (high-intensity) */
                    console->attribs |= 0x08;
                    break;
                case 4:    /* underscore */
                    break;
                case 5:    /* blink */
                    console->attribs |= 0x80;
                    break;
                case 7:    /* reverse video */
                    console->attribs = (console->attribs & 0x0f) >> 8 |
                        (console->attribs & 0x0f) << 8;
                    break;
                case 8:    /* concealed */
                    console->attribs = (console->attribs & 0x0f) |
                        (console->attribs & 0x0f) << 8;
                    break;
                }
            }
            else if (esc[i] >= 30 && esc[i] <= 37)
                /* foreground */
                console->attribs = (console->attribs & 0xf0) | 
                    ansi_to_vga[esc[i] - 30];
            else if (esc[i] >= 40 && esc[i] <= 47)
                /* background */
                console->attribs = (console->attribs & 0x0f) | 
                    (ansi_to_vga[esc[i] - 40] << 4);
        }
        break;

    case 'h':    /* ESC[=psh - set mode */
    case 'l':    /* ESC[=Psl - reset mode */
    case 'p':    /* ESC[code;string;...p - set keyboard strings */
        /* not supported */
        break;
    }
}

void ConClientThread(void)
{
    char buf[160];
    console_t *console;
    size_t bytes;
    unsigned i, writeptr, count;
    wchar_t wc;

    console = ThrGetThreadInfo()->param;
    if (current == NULL)
        current = console;

    ConClear(console);
    if (console->buffer == current->buffer)
        ConBlitToScreen(&console->rect);

    writeptr = 0;
tryagain:
    while (FsReadSync(console->client, buf + writeptr, sizeof(buf) - writeptr, &bytes))
    {
        MuxAcquire(mux_draw);
        ConDrawCursor(console, false);
        for (i = 0; i < bytes; i += count)
        {
            count = 0;
            if ((buf[i] & 0x80) == 0)
                count = 1;
            else if ((buf[i] & 0xE0) == 0xC0)
                count = 2;
            else if ((buf[i] & 0xF0) == 0xE0)
                count = 3;

            if (i + count > bytes)
            {
                memmove(buf, buf + i, sizeof(buf) - i);
                writeptr = i + bytes;
                goto tryagain;
            }

            if (count == 0)
            {
                wc = '?';
                count = 1;
            }
            else
                mbtowc(&wc, buf + i, count);

            switch (console->escape)
            {
            case 0:    /* normal character */
                switch (wc)
                {
                case 27:
                    console->escape++;
                    break;
                case '\t':
                    console->x = (console->x + 4) & ~3;
                    if (console->x >= console->width)
                    {
                        console->x = 0;
                        console->y++;
                        ConScroll(console);
                    }
                    ConUpdateCursor(console);
                    break;
                case '\n':
                    console->x = 0;
                    console->y++;
                    ConScroll(console);
                    ConUpdateCursor(console);
                    break;
                case '\r':
                    console->x = 0;
                    ConUpdateCursor(console);
                    break;
                case '\b':
                    if (console->x > 0)
                    {
                        console->x--;
                        ConUpdateCursor(console);
                    }
                    break;
                default:
                    console->buf_chars[console->x + console->y * console->width] = wc;
                    console->buf_attribs[console->x + console->y * console->width] = console->attribs;
                    if (console->buffer == current->buffer)
                        ConWriteChar(console->rect.left + console->x * char_width, 
                            console->rect.top + console->y * char_height, 
                            wc,
                            console->attribs & 0x0f,
                            (console->attribs >> 4) & 0x0f);
                    console->x++;
                }
                break;

            case 1:    /* after ESC */
                if (wc == '[')
                {
                    console->escape++;
                    console->esc_param = 0;
                    console->esc[0] = 0;
                }
                else
                {
                    console->escape = 0;
                    continue;
                }
                break;

            case 2:    /* after ESC[ */
            case 3:    /* after ESC[n; */
            case 4:    /* after ESC[n;n; */
                if (iswdigit(wc))
                {
                    console->esc[console->esc_param] = 
                        console->esc[console->esc_param] * 10 + wc - '0';
                    break;
                }
                else if (wc == ';' &&
                    console->esc_param < _countof(console->esc) - 1)
                {
                    console->esc_param++;
                    console->esc[console->esc_param] = 0;
                    console->escape++;
                    break;
                }
                else
                {
                    console->escape = 10;
                    /* fall through */
                }

            case 10:    /* after all parameters */
                /*wprintf(L"esc: %c: %u/%u/%u\n", 
                    *str, 
                    esc[0],
                    esc[1],
                    esc[2]);*/
                ConDoEscape(console, wc, console->esc_param + 1, console->esc);
                console->escape = 0;
                break;
            }

            if (console->x >= console->width)
            {
                console->x = 0;
                console->y++;
            }

            ConScroll(console);
        }

        writeptr = 0;
        ConUpdateCursor(console);
        if (console->buffer == current->buffer)
            ConBlitToScreen(&console->rect);
        ConDrawCursor(console, true);
        MuxRelease(mux_draw);
    }

    HndClose(console->client);
    if (current == console)
        current = NULL;

    ThrExitThread(0);
}

void ConTileBuffer(unsigned num)
{
    unsigned i, count, height;

    count = 0;
    for (i = 0; i < num_consoles; i++)
        if (consoles[i].buffer == num)
            count++;

    if (count == 0)
        return;

    height = mode.height / count;
    for (i = 0; i < num_consoles; i++)
    {
        if (consoles[i].buffer == num)
        {
            consoles[i].rect.left = 0;
            consoles[i].rect.right = mode.width;
            consoles[i].rect.top = i * height;
            consoles[i].rect.bottom = (i + 1) * height;
            consoles[i].width = 
                (consoles[i].rect.right - consoles[i].rect.left) / char_width;
            consoles[i].height = 
                (consoles[i].rect.bottom - consoles[i].rect.top) / char_height;
        }
    }
}

void mainCRTStartup(void)
{
    handle_t server, client, vidmem, vid;
    const wchar_t *server_name = SYS_PORTS L"/console",
        *shell_name = L"/fd/mobius/shell.exe";
	//*shell_name = L"/System/Boot/shell.exe";
    process_info_t defaults = { 0 };
    unsigned i;
    params_vid_t params;

    vid = FsOpen(SYS_DEVICES L"/Classes/video0", FILE_READ | FILE_WRITE);
    if (vid == NULL)
    {
        ProcExitProcess(1);
        return;
    }

    memset(&mode, 0, sizeof(mode));
    mode.bitsPerPixel = 8;

    params.vid_setmode = mode;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), NULL))
    {
        HndClose(vid);
        ProcExitProcess(2);
        return;
    }

    mode = params.vid_setmode;
    mux_draw = MuxCreate();

    for (i = 0; i < _countof(consoles); i++)
    {
        consoles[i].width = mode.width / char_width;
        consoles[i].height = mode.height / char_height;
        consoles[i].default_attribs = consoles[i].attribs = (i + 1) << 4 | 0x07;
        consoles[i].buffer = 0;

        consoles[i].buf_attribs = malloc(consoles[i].width * consoles[i].height);
        memset(consoles[i].buf_attribs, 
            consoles[i].default_attribs,
            consoles[i].width * consoles[i].height);

        consoles[i].buf_chars = malloc(sizeof(wchar_t) * consoles[i].width * consoles[i].height);
        memset(consoles[i].buf_chars, 0, sizeof(wchar_t) * consoles[i].width * consoles[i].height);
    }

    num_consoles = 1;
    ConTileBuffer(0);
    num_consoles = 0;

    current = consoles;
    vidmem = VmmOpenSharedArea(mode.framebuffer);
    video_mem = VmmMapSharedArea(vidmem, NULL, 
        VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
    buffer_mem = malloc(mode.bytesPerLine * mode.height);
    mem = buffer_mem;

    server = FsCreate(server_name, 0);

    ThrCreateThread(ConKeyboardThread, NULL, 14);
    ThrCreateThread(ConCursorThread, NULL, 16);

    for (i = 0; i < 1; i++)
    {
        client = FsOpen(server_name, FILE_READ | FILE_WRITE);
        defaults.std_in = defaults.std_out = client;
        wcscpy(defaults.cwd, L"/");
        if (ProcSpawnProcess(shell_name, &defaults) == NULL)
		{
			char *str;
			str = strerror(errno);
			FsWriteSync(client, str, strlen(str), NULL);
		}
    }

    while ((client = PortAccept(server, FILE_READ | FILE_WRITE)))
    {
        if (num_consoles < _countof(consoles))
        {
            consoles[num_consoles].client = client;
            ThrCreateThread(ConClientThread, consoles + num_consoles, 15);
            num_consoles++;
        }
        else
            HndClose(client);
    }

    free(buffer_mem);
    HndClose(mux_draw);
    HndClose(server);
    HndClose(vidmem);
    HndClose(vid);
    ProcExitProcess(EXIT_SUCCESS);
}
