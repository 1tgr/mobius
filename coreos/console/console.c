/*
 * $History: console.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 14:11
 * Updated in $/coreos/console
 * Reinstated code in ConClientThread
 * Tidied up
 * Added history block
 */

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "internal.h"
#include <os/port.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <os/ioctl.h>
#include <os/keyboard.h>
#include <os/video.h>
#include <os/framebuf.h>


#define CURSOR_HEIGHT 2

console_t consoles[2];
void *cookies[1];
unsigned num_consoles;
lmutex_t lmux_consoles;
unsigned num_buffers;
console_t *current;

videomode_t mode;
handle_t vid;
void *vidmem;
surface_t surf;


void ConDrawCursor(console_t *console, bool do_draw)
{
    if (do_draw != console->cursor_visible)
    {
        rect_t rect;
        unsigned i, pitch;
        uint8_t *bitmap;

        rect.left = console->rect.left + console->x * console->char_width;
        rect.bottom = console->rect.top + (console->y + 1) * console->char_height;
        rect.right = rect.left + console->char_width;
        rect.top = rect.bottom - CURSOR_HEIGHT;

        pitch = ((mode.bitsPerPixel * console->char_width + 7) & -8) / 8;
        bitmap = malloc(pitch * CURSOR_HEIGHT);
        surf.vtbl->SurfBltScreenToMemory(&surf, bitmap, pitch, &rect);
        for (i = 0; i < pitch * CURSOR_HEIGHT; i++)
            bitmap[i] = ~bitmap[i];
        surf.vtbl->SurfBltMemoryToScreen(&surf, &rect, bitmap, pitch);
        free(bitmap);

        surf.vtbl->SurfFlush(&surf);

        console->cursor_visible = do_draw;
    }
}


int ConKeyboardThread(void *param)
{
    uint32_t ch, code;
    handle_t keyboard;
    //void *old_buffer;
    //unsigned i;

    keyboard = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
    while (FsRead(keyboard, &ch, 0, sizeof(ch), NULL))
    {
        code = ch & ~KBD_BUCKY_ANY;
        if ((ch & KBD_BUCKY_ALT) != 0 &&
            code >= KEY_F1 && 
            code <= KEY_F12)
        {
            /*LmuxAcquire(&lmux_consoles);
            LmuxAcquire(&current->lmux);
            ConDrawCursor(current, false);

            old_buffer = current->cookie;
            current = consoles + code - KEY_F1;

            if (old_buffer != current->buffer)
                for (i = 0; i < num_consoles; i++)
                    if (consoles[i].buffer == current->buffer)
                        ConRedraw(consoles + i);

            ConDrawCursor(current, true);
            LmuxRelease(&current->lmux);
            LmuxRelease(&lmux_consoles);*/
        }
        else if (ch == (KBD_BUCKY_CTRL | KBD_BUCKY_ALT | KEY_DEL))
            SysShutdown(SHUTDOWN_REBOOT);
        else if (ch == (KBD_BUCKY_ALT | '\t') || 
            ch == (KBD_BUCKY_ALT | KBD_BUCKY_SHIFT | '\t'))
        {
            LmuxAcquire(&lmux_consoles);

            LmuxAcquire(&current->lmux);
            ConDrawCursor(current, false);
            LmuxRelease(&current->lmux);

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

            LmuxAcquire(&current->lmux);
            ConDrawCursor(current, true);
            LmuxRelease(&current->lmux);

            LmuxRelease(&lmux_consoles);
        }
        else
        {
            LmuxAcquire(&lmux_consoles);
            if (current != NULL && current->client != NULL)
                FsWrite(current->client, &ch, 0, sizeof(ch), NULL);
            LmuxRelease(&lmux_consoles);
        }
    }

    _wdprintf(L"console(keyboard): FsRead failed, %s\n", _wcserror(errno));
    return errno;
}


int ConCursorThread(void *param)
{
    while (true)
    {
        LmuxAcquire(&lmux_consoles);
        if (current != NULL)
        {
            LmuxAcquire(&current->lmux);
            ConDrawCursor(current, !current->cursor_visible);
            LmuxRelease(&current->lmux);
        }
        LmuxRelease(&lmux_consoles);

        ThrSleep(250);
    }
}


void ConScroll(console_t *console)
{
    unsigned y;
    rect_t src, dest;

    if (console->y > console->height - 1)
    {
        if (console->cookie == current->cookie)
        {
            y = console->y;
            src = console->rect;
            src.right = src.left + console->width * console->char_width;
            src.bottom = src.top + console->height * console->char_height;
            dest = src;
            src.top += console->char_height;
            dest.bottom -= console->char_height;

            while (y > console->height - 1)
            {
                surf.vtbl->SurfBltScreenToScreen(&surf, &dest, &src);
                surf.vtbl->SurfFillRect(&surf, 
                    dest.left, dest.bottom,
                    dest.right, src.bottom,
                    console->bg_colour);
                y--;
            }
        }

        console->y = console->height - 1;
    }
}


void ConClear(console_t *console)
{
    if (console->cookie == current->cookie)
        surf.vtbl->SurfFillRect(&surf,
            console->rect.left, console->rect.top,
            console->rect.right, console->rect.bottom,
            console->bg_colour);

    console->x = console->y = 0;
}


void ConClearEol(console_t *console)
{
    if (console->cookie == current->cookie)
        surf.vtbl->SurfFillRect(&surf,
            console->rect.left + console->x * console->char_width, 
            console->rect.top + console->y * console->char_height,
            console->rect.right,
            console->rect.top + (console->y + 1) * console->char_height,
            console->bg_colour);
}


void ConBeginOutput(console_t *console)
{
    console->str_x = console->x;
    console->str_y = console->y;
}


void ConPutChar(console_t *console, wchar_t ch)
{
    console->buf_chars[console->x + console->y * console->width] = ch;
    console->x++;
}


void ConEndOutput(console_t *console)
{
    assert(console->x >= console->str_x);
    assert(console->y == console->str_y);

    if (console->x != console->str_x)
    {
        if (console->cookie == current->cookie)
        {
            FontDrawText(console->font,
                &surf,
                console->rect.left + console->str_x * console->char_width,
                console->rect.top + console->str_y * console->char_height,
                console->buf_chars + console->str_x + console->str_y * console->width,
                console->x - console->str_x,
                console->fg_colour,
                console->bg_colour);
        }
    }
}


static colour_t ConColourLighter(colour_t colour)
{
    uint8_t red, green, blue;

    red = COLOUR_RED(colour);
    if (red >= 0x80)
        red = 0xff;
    else
        red = 0x80;

    green = COLOUR_GREEN(colour);
    if (green >= 0x80)
        green = 0xff;
    else
        green = 0x80;

    blue = COLOUR_BLUE(colour);
    if (blue >= 0x80)
        blue = 0xff;
    else
        blue = 0x80;

    return MAKE_COLOUR(red, green, blue);
}


void ConDoEscape(console_t *console, int code, unsigned num, const unsigned *esc)
{
    unsigned i;
    char buf[20];
    colour_t temp_colour;
    static const colour_t ansi_to_colour[] =
    {
        0x000000, /* black */
        0x800000, /* red */
        0x008000, /* green */
        0x808000, /* yellow */
        0x000080, /* blue */
        0x800080, /* magenta */
        0x008080, /* cyan */
        0xC0C0C0, /* white */
    };

    switch (code)
    {
    case 'H':    /* ESC[PL;PcH - cursor position */
    case 'f':    /* ESC[PL;Pcf - cursor position */
        if (num > 0)
            console->y = min(esc[0], console->height - 1);
        if (num > 1)
            console->x = min(esc[1], console->width - 1);
        break;
    
    case 'A':    /* ESC[PnA - cursor up */
        if (num > 0 && console->y >= esc[0])
            console->y -= esc[0];
        break;
    
    case 'B':    /* ESC[PnB - cursor down */
        if (num > 0 && console->y < console->height + esc[0])
            console->y += esc[0];
        break;

    case 'C':    /* ESC[PnC - cursor forward */
        if (num > 0 && console->x < console->width + esc[0])
            console->x += esc[0];
        break;
    
    case 'D':    /* ESC[PnD - cursor backward */
        if (num > 0 && console->x >= esc[0])
            console->x -= esc[0];
        break;

    case 's':    /* ESC[s - save cursor position */
        console->save_x = console->x;
        console->save_y = console->y;
        break;

    case 'u':    /* ESC[u - restore cursor position */
        console->x = console->save_x;
        console->y = console->save_y;
        break;

    case 'J':    /* ESC[2J - clear screen */
        ConClear(console);
        break;

    case 'K':    /* ESC[K - erase line */
        ConClearEol(console);
        break;

    case 'm':    /* ESC[Ps;...Psm - set graphics mode */
        if (num == 0)
        {
            console->fg_colour = console->default_fg_colour;
            console->bg_colour = console->default_bg_colour;
        }
        else for (i = 0; i < num; i++)
        {
            if (esc[i] <= 8)
            {
                switch (esc[i])
                {
                case 0:    /* all attributes off */
                    console->fg_colour = console->default_fg_colour;
                    console->bg_colour = console->default_bg_colour;
                    break;
                case 1:    /* bold (high-intensity) */
                    console->fg_colour = ConColourLighter(console->default_fg_colour);
                    break;
                case 4:    /* underscore */
                    break;
                case 5:    /* blink */
                    console->bg_colour = ConColourLighter(console->default_bg_colour);
                    break;
                case 7:    /* reverse video */
                    temp_colour = console->bg_colour;
                    console->bg_colour = console->fg_colour;
                    console->fg_colour = temp_colour;
                    break;
                case 8:    /* concealed */
                    console->fg_colour = console->bg_colour;
                    break;
                }
            }
            else if (esc[i] >= 30 && esc[i] <= 37)
                /* foreground */
                console->default_fg_colour = console->fg_colour = ansi_to_colour[esc[i] - 30];
            else if (esc[i] >= 40 && esc[i] <= 47)
                /* background */
                console->default_bg_colour = console->bg_colour = ansi_to_colour[esc[i] - 40];
        }
        break;

    case 'h':   /* ESC[=psh - set mode */
    case 'l':   /* ESC[=Psl - reset mode */
    case 'p':   /* ESC[code;string;...p - set keyboard strings */
        /* not supported */
        break;

    case 'n':   /* ESC[Pnn - DSR - DEVICE STATUS REPORT */
        switch (esc[0])
        {
        case 6:
            /* send CPR - ACTIVE POSITION REPORT */
            FsWrite(console->client, buf, 0, 
                sprintf(buf, "\x1b[%u;%uR", console->y, console->x),
                NULL);
            break;
        }
        break;
    }
}


void ConPutStr(console_t *console, const wchar_t *str, size_t length)
{
    wchar_t wc;

    LmuxAcquire(&console->lmux);

    if (console == current)
        ConDrawCursor(console, false);

    ConBeginOutput(console);

    while (length > 0)
    {
        wc = *str;

        switch (console->escape)
        {
        case 0:    /* normal character */
            switch (wc)
            {
            case 27:
                ConEndOutput(console);
                console->escape++;
                break;
            case '\t':
                ConEndOutput(console);
                console->x = (console->x + 4) & ~3;
                if (console->x >= console->width)
                {
                    console->x = 0;
                    console->y++;
                    ConScroll(console);
                }
                ConBeginOutput(console);
                break;
            case '\n':
                ConEndOutput(console);
                console->x = 0;
                console->y++;
                ConScroll(console);
                ConBeginOutput(console);
                break;
            case '\r':
                ConEndOutput(console);
                console->x = 0;
                ConBeginOutput(console);
                break;
            case '\b':
                ConEndOutput(console);
                if (console->x > 0)
                    console->x--;
                ConBeginOutput(console);
                break;
            default:
                ConPutChar(console, wc);
                break;
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
                ConBeginOutput(console);
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
            ConDoEscape(console, wc, console->esc_param + 1, console->esc);
            console->escape = 0;
            ConBeginOutput(console);
            break;
        }

        if (console->x >= console->width)
        {
            ConEndOutput(console);
            console->x = 0;
            console->y++;
            ConBeginOutput(console);
        }

        if (console->y > console->height - 1)
        {
            ConEndOutput(console);
            ConScroll(console);
            ConBeginOutput(console);
        }

        str++;
        length--;
    }

    ConEndOutput(console);

    if (console == current)
        ConDrawCursor(console, true);

    LmuxRelease(&console->lmux);
}


typedef struct con_mbstate_t con_mbstate_t;
struct con_mbstate_t
{
    char spare[2];
    int num_spare;
};


void ConInitMbState(con_mbstate_t *state)
{
    state->spare[0] = state->spare[1] = '\0';
    state->num_spare = 0;
}


bool ConIsLeadByte(char ch)
{
    return ((ch & 0x80) == 0) ||
        ((ch & 0xE0) == 0xC0) ||
        ((ch & 0xF0) == 0xE0);
}


size_t ConMultiByteToWideChar(wchar_t *dest, const char *src, size_t src_bytes, con_mbstate_t *state)
{
    int i;
    unsigned char b[3];
    unsigned count;
    size_t dest_chars;

    if (state->num_spare > 0)
        assert(ConIsLeadByte(state->spare[0]));

    switch (state->num_spare)
    {
    case 0:
        if (src_bytes > 0)
            b[0] = src[0];
        if (src_bytes > 1)
            b[1] = src[1];
        if (src_bytes > 2)
            b[2] = src[2];
        break;

    case 1:
        b[0] = state->spare[0];
        if (src_bytes > 0)
            b[1] = src[0];
        if (src_bytes > 1)
            b[2] = src[1];
        break;

    case 2:
        b[0] = state->spare[0];
        b[1] = state->spare[1];
        if (src_bytes > 0)
            b[2] = src[0];
        break;
    }

    dest_chars = 0;
    i = -state->num_spare;
    state->num_spare = 0;
    while (i < (int) src_bytes)
    {
        if ((b[0] & 0x80) == 0)
            count = 1;
        else if ((b[0] & 0xE0) == 0xC0)
            count = 2;
        else if ((b[0] & 0xF0) == 0xE0)
            count = 3;
        else
        {
            _wdprintf(L"ConMultiByteToWideChar: invalid lead byte: %02x = %c\n",
                b[0], b[0]);
            count = 1;
        }

        if (i + count > src_bytes)
        {
            int j;

            state->num_spare = (int) src_bytes - i;
            assert(state->num_spare <= _countof(state->spare));
            for (j = 0; j < state->num_spare; j++)
                state->spare[j] = src[i + j];

            break;
        }

        i += count;

        switch (count)
        {
        case 1:
            *dest = (wchar_t) b[0];
            b[0] = b[1];
            b[1] = b[2];
            b[2] = src[i + 2];
            break;

        case 2:
            *dest = (b[0] & 0x1f) << 6 | 
                (b[1] & 0x3f);
            b[0] = b[2];
            b[1] = src[i + 1];
            b[2] = src[i + 2];
            break;

        case 3:
            *dest = (b[0] & 0x0f) << 12 | 
                (b[1] & 0x3f) << 6 | 
                (b[2] & 0x3f);
            b[0] = src[i + 0];
            b[1] = src[i + 1];
            b[2] = src[i + 2];
            break;
        }

        dest_chars++;
        dest++;
    }

    return dest_chars;
}


int ConClientThread(void *param)
{
    char buf[80];
    console_t *console;
    size_t bytes, length;
    wchar_t wbuf[80];
    con_mbstate_t state;

    console = param;
    LmuxAcquire(&lmux_consoles);
    if (current == NULL)
        current = console;
    LmuxRelease(&lmux_consoles);

    ConInitMbState(&state);
    while (FsRead(console->client, buf, 0, sizeof(buf), &bytes))
    {
        length = ConMultiByteToWideChar(wbuf, buf, bytes, &state);
        if (length > 0)
            ConPutStr(console, wbuf, length);
    }

    HndClose(console->client);
    LmuxAcquire(&lmux_consoles);
    if (current == console)
        current = NULL;
    LmuxRelease(&lmux_consoles);

    return 0;
}


void ConTileBuffer(unsigned num)
{
    unsigned i, count, height, old_length, new_length;

    count = 0;
    for (i = 0; i < num_consoles; i++)
        if (consoles[i].cookie == cookies[num])
            count++;

    if (count == 0)
        return;

    height = mode.height / count;
    for (i = 0; i < num_consoles; i++)
    {
        if (consoles[i].cookie == cookies[num])
        {
            consoles[i].rect.left = 0;
            consoles[i].rect.right = mode.width;
            consoles[i].rect.top = i * height;
            consoles[i].rect.bottom = (i + 1) * height;

            old_length = consoles[i].width * consoles[i].height;
            consoles[i].width = (consoles[i].rect.right - consoles[i].rect.left) / consoles[i].char_width;
            consoles[i].height = (consoles[i].rect.bottom - consoles[i].rect.top) / consoles[i].char_height;

            new_length = consoles[i].width * consoles[i].height;
            consoles[i].buf_chars = realloc(consoles[i].buf_chars, sizeof(wchar_t) * new_length);

            if (new_length > old_length)
            {
                memset(consoles[i].buf_chars + old_length, 
                    0, 
                    sizeof(wchar_t) * (new_length - old_length));
            }
        }
    }
}


#if 0
/*
 * xxx -- windres has virtually no Unicode support, so no escape sequences 
 *  in resource files
 */
void ConDisplaySignonMessage(void)
{
    addr_t base;
    size_t length;
    wchar_t *message;

    base = ProcGetProcessInfo()->base;
    length = ResGetStringLength(base, 1);
    message = malloc(sizeof(wchar_t) * (length + 1));
    if (message != NULL)
    {
        ResLoadString(base, 1, message, length);
        ConPutStr(consoles + 0, message, length);
        free(message);
    }
}
#else

#define A   L"\x2554"   /* double top left */
#define B   L"\x2550"   /* double horizontal */
#define C   L"\x2557"   /* double top right */
#define D   L"\x2551"   /* double vertical */
#define E   L"\x255f"   /* double vertical to right single */
#define F   L"\x2500"   /* single horizontal */
#define G   L"\x2562"   /* left single to double vertical */
#define H   L"\x255a"   /* double bottom left */
#define I   L"\x255d"   /* double bottom right */
#define _   L"\x2022"   /* bullet */

static const wchar_t signon_message[] =
A B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B C L"\n"
D L"     " L"\x1b[1m" L"Welcome to The Möbius operating system - http://www.themobius.co.uk/" L"\x1b[0m" L"    " D L"\n"
E F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F F G L"\n"
D L" Two shells are currently running under the control of the console server.   " D L"\n"
D L"                                                                             " D L"\n"
D L"  " _ L" To switch between the two consoles, press Alt+Tab                        " D L"\n"
D L"  " _ L" For a list of commands, type help                                        " D L"\n"
//D L"  " _ L" To start the GUI, run winmgr in /Mobius                                  " D L"\n"
D L"  " _ L" To play Tetris, run tetris in /Mobius                                    " D L"\n"
D L"  " _ L" To start the kernel debugger, press F11                                  " D L"\n"
H B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B B I L"\n"
L"\n";

#undef A
#undef B
#undef C
#undef D
#undef E
#undef F
#undef G
#undef H
#undef I
#undef _

void ConDisplaySignonMessage(void)
{
    wchar_t message[100], colours[20];

    ConPutStr(consoles + 0, 
        signon_message, 
        _countof(signon_message) - 1);

    switch (mode.bitsPerPixel)
    {
    case 15:
    case 16:
        wcscpy(colours, L"high colour");
        break;

    case 24:
    case 32:
        wcscpy(colours, L"true colour");
        break;

    default:
        swprintf(colours, L"%u colours", 1 << mode.bitsPerPixel);
        break;
    }

    ConPutStr(consoles + 0, 
        message, 
        swprintf(message, L"Graphics resolution is %ux%u, %s\n", 
            mode.width, mode.height, colours));
}
#endif


static const wchar_t server_name[] = SYS_PORTS L"/console";
static const wchar_t font_name[] = L"/Mobius/veramono.ttf";

int mainCRTStartup(void)
{
    handle_t server, client;
    unsigned i;
    params_vid_t params;
    font_t *font;

    vid = FsOpen(SYS_DEVICES L"/Classes/video0", FILE_READ | FILE_WRITE);
    if (vid == NULL)
    {
        _wdprintf(L"console: " SYS_DEVICES L"/Classes/video0: %s\n", _wcserror(errno));
        return errno;
    }

    memset(&mode, 0, sizeof(mode));
    params.vid_setmode = mode;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), NULL))
    {
        _wdprintf(L"console: failed to set video mode: %s\n", _wcserror(errno));
        HndClose(vid);
        return errno;
    }

    mode = params.vid_setmode;

    if (mode.flags & VIDEO_MODE_TEXT)
    {
        _wdprintf(L"console: text mode not supported\n");
        return 0;
    }

    if (mode.bitsPerPixel == 4)
    {
        vidmem = NULL;
        if (!AccelCreateSurface(&mode, vid, &surf))
        {
            _wdprintf(L"console: AccelCreateSurface failed\n");
            return errno;
        }
    }
    else
    {
        handle_t handle_vidmem;

        handle_vidmem = HndOpen(mode.framebuffer);
        if (handle_vidmem == 0)
        {
            _wdprintf(L"console: unable to open framebuffer %s\n", mode.framebuffer);
            return errno;
        }

        vidmem = VmmMapSharedArea(handle_vidmem, 0, 
            VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
        HndClose(handle_vidmem);

        if (!FramebufCreateSurface(&mode, vidmem, &surf))
        {
            _wdprintf(L"console: video mode not supported: %u bits per pixel\n", mode.bitsPerPixel);
            return errno;
        }
    }

    FontInit();
    font = FontLoad(font_name, 10 * 64, 
        mode.bitsPerPixel < 8 ? FB_FONT_MONO : FB_FONT_SMOOTH);
    if (font == NULL)
    {
        _wdprintf(L"console: failed to load font %s\n", font_name);
        return errno;
    }

    cookies[0] = (void*) 0x12345678;

    num_buffers = 1;

    LmuxInit(&lmux_consoles);
    for (i = 0; i < _countof(consoles); i++)
    {
        LmuxInit(&consoles[i].lmux);
        consoles[i].width = consoles[i].height = 0;
        consoles[i].fg_colour = consoles[i].default_fg_colour = 0xc0c0c0;
        consoles[i].bg_colour = consoles[i].default_bg_colour = 0x000000;
        consoles[i].cookie = cookies[0];
        consoles[i].buf_chars = NULL;
        consoles[i].font = font;
        FontGetMaxSize(consoles[i].font, 
            &consoles[i].char_width, &consoles[i].char_height);
    }

    num_consoles = _countof(consoles);
    ConTileBuffer(0);
    num_consoles = 0;

    current = consoles;

    for (i = 0; i < _countof(consoles); i++)
        ConClear(consoles + i);

    server = FsCreate(server_name, 0);

    ThrCreateThread(ConKeyboardThread, NULL, 16, L"ConKeyboardThread");
    ThrCreateThread(ConCursorThread, NULL, 16, L"ConCursorThread");

    ConDisplaySignonMessage();

    while ((client = PortAccept(server, FILE_READ | FILE_WRITE)))
    {
        if (num_consoles < _countof(consoles))
        {
            consoles[num_consoles].client = client;
            ThrCreateThread(ConClientThread, consoles + num_consoles, 15, L"ConClientThread");
            num_consoles++;
        }
        else
            HndClose(client);
    }

    for (i = 0; i < _countof(consoles); i++)
    {
        LmuxDelete(&consoles[i].lmux);
        free(consoles[i].buf_chars);
    }

    LmuxDelete(&lmux_consoles);
    FontDelete(font);
    HndClose(server);
    if (vidmem != NULL)
        VmmFree(vidmem);
    HndClose(vid);
    return EXIT_SUCCESS;
}
