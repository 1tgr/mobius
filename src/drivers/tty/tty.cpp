#include <kernel/kernel.h>
#include <kernel/arch.h>

#include <kernel/device>
using namespace kernel;

#include <kernel/io.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* in map.c */
extern "C" size_t wcsto437(char *mbstr, const wchar_t *wcstr, size_t count);

extern "C" void TtySwitchConsoles(unsigned n);

typedef struct tty tty;
class tty : public device
{
public:
    DEV_BEGIN_REQUEST()
        DEV_HANDLE_REQUEST(DEV_WRITE_DIRECT, onWriteDirect, request_dev_t)
    DEV_END_REQUEST()

    bool isr(uint8_t irq);
    void finishio(request_t *req);

    tty();
    void switchTo();
    void clear();
    
protected:
    dirent_device_t m_info;

    void updateCursor();
    void scroll();
    void flush();
    void doEscape(char code, unsigned num, const unsigned *esc);
    void writeString(const char *str, size_t count);

    bool onWriteDirect(request_dev_t *req);

    uint16_t *buf_top;
    uint16_t attribs;
    unsigned x, y, width, height, save_x, save_y;
    unsigned escape, esc[3], esc_param;
    const char *writebuf;
    size_t writeptr;
};

unsigned num_consoles = 1;
semaphore_t sem_consoles;
tty consoles[12];
unsigned cur_console;

#define _crtc_base_adr    0x3C0

/* I/O addresses for VGA registers */
#define    VGA_MISC_READ    0x3CC
#define    VGA_GC_INDEX    0x3CE
#define    VGA_GC_DATA        0x3CF
#define    VGA_CRTC_INDEX    0x14 /* relative to 0x3C0 or 0x3A0 */
#define    VGA_CRTC_DATA    0x15 /* relative to 0x3C0 or 0x3A0 */

void memset16(uint16_t *ptr, uint16_t c, size_t count)
{
    while (count-- > 0)
        *ptr++ = c;
}

tty::tty()
{
    unsigned id = this - consoles;
    buf_top = (uint16_t*) PHYSICAL(0xb8000) + 80 * 25 * id;
    swprintf(m_info.description, L"Text console at %p", 
	(uint8_t*) 0xb8000 + 80 * 25 * id);
    m_info.device_class = 0;
    info = &m_info;
    flags = DEVICE_IO_DIRECT;
    attribs = 0x0700 | id << 12;
    x = 0;
    y = 0;
    width = 80;
    height = 25;
    escape = 0;
    writeptr = 0;
    writebuf = NULL;
}

void tty::updateCursor()
{
    uint16_t addr;
    
    if ((unsigned) (this - consoles) == cur_console)
    {
        addr = (uint16_t) (addr_t) (buf_top - (uint16_t*) PHYSICAL(0xb8000)) 
            + y * width + x;
        out(_crtc_base_adr + VGA_CRTC_INDEX, 14);
        out(_crtc_base_adr + VGA_CRTC_DATA, addr >> 8);
        out(_crtc_base_adr + VGA_CRTC_INDEX, 15);
        out(_crtc_base_adr + VGA_CRTC_DATA, addr);
    }
}

void tty::switchTo()
{
    uint16_t addr;
    addr = (uint16_t) (addr_t) (buf_top - (uint16_t*) PHYSICAL(0xb8000));
    
    out(_crtc_base_adr + VGA_CRTC_INDEX, 12);
    out(_crtc_base_adr + VGA_CRTC_DATA, addr >> 8);
    out(_crtc_base_adr + VGA_CRTC_INDEX, 13);
    out(_crtc_base_adr + VGA_CRTC_DATA, addr);
    cur_console = this - consoles;

    updateCursor();
}

void tty::scroll()
{
    uint16_t *mem;
    unsigned i;

    while (y >= height)
    {
        mem = buf_top;
        memmove(mem, mem + width, width * (height - 1) * 2);
        mem += width * (height - 1);
        for (i = 0; i < width; i++)
            mem[i] = attribs | ' ';

        y--;
    }

    updateCursor();
}

void tty::flush()
{
    wchar_t wc[81];
    char mb[81], *ch;
    size_t count;
    uint16_t *out;

    memset(mb, 0, sizeof(mb));
    strncpy(mb, writebuf, min(_countof(mb) - 1, writeptr));
    count = mbstowcs(wc, mb, _countof(wc) - 1);
    if (count == (size_t) -1)
        wc[0] = '\0';
    else
        wc[count] = '\0';
    count = wcsto437(mb, wc, _countof(mb) - 1);
    out = buf_top + width * y + x;
    ch = mb;
    while (count > 0)
    {
        *out++ = attribs | (uint8_t) *ch++;
        count--;

        x++;
        if (x >= width)
        {
            x = 0;
            y++;
            out = buf_top + width * y + x;
        }
        
        if (y >= height)
        {
            scroll();
            out = buf_top + width * y + x;
        }
    }

    writebuf += writeptr;
    writeptr = 0;
    updateCursor();
}

void tty::clear()
{
    memset16(buf_top, attribs | ' ', width * height);
    x = 0;
    y = 0;
    updateCursor();
}

void tty::doEscape(char code, unsigned num, const unsigned *esc)
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
            y = min(esc[0], height - 1);
        if (num > 1)
            x = min(esc[1], width - 1);
        updateCursor();
        break;
    
    case 'A':    /* ESC[PnA - cursor up */
        if (num > 0 && y >= esc[0])
        {
            y -= esc[0];
            updateCursor();
        }
        break;
    
    case 'B':    /* ESC[PnB - cursor down */
        if (num > 0 && y < height + esc[0])
        {
            y += esc[0];
            updateCursor();
        }
        break;

    case 'C':    /* ESC[PnC - cursor forward */
        if (num > 0 && x < width + esc[0])
        {
            x += esc[0];
            updateCursor();
        }
        break;
    
    case 'D':    /* ESC[PnD - cursor backward */
        if (num > 0 && y >= esc[0])
        {
            x -= esc[0];
            updateCursor();
        }
        break;

    case 's':    /* ESC[s - save cursor position */
        save_x = x;
        save_y = y;
        break;

    case 'u':    /* ESC[u - restore cursor position */
        x = save_x;
        y = save_y;
        updateCursor();
        break;

    case 'J':    /* ESC[2J - clear screen */
        clear();
        break;

    case 'K':    /* ESC[K - erase line */
	memset16(buf_top + width * y + x,
            attribs | ' ',
            width - x);
        break;

    case 'm':    /* ESC[Ps;...Psm - set graphics mode */
        for (i = 0; i < num; i++)
        {
            if (esc[i] < 8)
            {
                switch (esc[i])
                {
                case 0:    /* all attributes off */
                    attribs &= 0x7700;
                    break;
                case 1:    /* bold (high-intensity) */
                    attribs |= 0x0800;
                    break;
                case 4:    /* underscore */
                    break;
                case 5:    /* blink */
                    attribs |= 0x8000;
                    break;
                case 7:    /* reverse video */
                    attribs = (attribs & 0x0f00) >> 8 |
                        (attribs & 0x0f00) << 8;
                    break;
                case 8:    /* concealed */
                    attribs = (attribs & 0x0f00) |
                        (attribs & 0x0f00) << 8;
                    break;
                }
            }
            else if (esc[i] >= 30 && esc[i] <= 37)
                /* foreground */
                attribs = (attribs & 0xf000) | 
                    (ansi_to_vga[esc[i] - 30] << 8);
            else if (esc[i] >= 40 && esc[i] <= 47)
                /* background */
                attribs = (attribs & 0x0f00) | 
                    (ansi_to_vga[esc[i] - 40] << 12);
        }
        break;

    case 'h':    /* ESC[=psh - set mode */
    case 'l':    /* ESC[=Psl - reset mode */
    case 'p':    /* ESC[code;string;...p - set keyboard strings */
        /* not supported */
        break;
    }
}

void tty::writeString(const char *str, size_t count)
{
    writebuf = str;
    writeptr = 0;
    while (count > 0)
    {
        switch (escape)
        {
        case 0:    /* normal character */
            switch (*str)
            {
            case 27:
                flush();
                escape++;
                break;
            case '\t':
                flush();
                writebuf = str + 1;
                x = (x + 4) & ~3;
                if (x >= width)
                {
                    x = 0;
                    y++;
                    scroll();
                }
                updateCursor();
                break;
            case '\n':
                flush();
                writebuf = str + 1;
                x = 0;
                y++;
                scroll();
                updateCursor();
                break;
            case '\b':
                flush();
                writebuf = str + 1;
                if (x > 0)
                {
                    x--;
                    updateCursor();
                }
                break;
            default:
                writeptr++;
                if (writeptr >= 80)
                    flush();
            }
            break;

        case 1:    /* after ESC */
            if (*str == '[')
            {
                escape++;
                esc_param = 0;
                esc[0] = 0;
            }
            else
            {
                escape = 0;
                continue;
            }
            break;

        case 2:    /* after ESC[ */
        case 3:    /* after ESC[n; */
        case 4:    /* after ESC[n;n; */
            if (iswdigit(*str))
                esc[esc_param] = 
                esc[esc_param] * 10 + *str - '0';
            else if (*str == ';' &&
                esc_param < _countof(esc) - 1)
            {
                esc_param++;
                esc[esc_param] = 0;
                escape++;
            }
            else
            {
                escape = 10;
                continue;
            }
            break;

        case 10:    /* after all parameters */
            /*wprintf(L"esc: %c: %u/%u/%u\n", 
                *str, 
                esc[0],
                esc[1],
                esc[2]);*/
            doEscape((char) *str, esc_param + 1, esc);
            escape = 0;
            writebuf = str + 1;
            writeptr = 0;
            break;
        }

        str++;
        count--;
    }

    flush();
}

bool tty::isr(uint8_t irq)
{
    uint8_t scan;
    scan = in(0x60);
    if (scan >= 0x3b && scan < 0x3b + 12)
    {
        consoles[scan - 0x3b].switchTo();
        return true;
    }
    else
        return false;
}

void tty::finishio(request_t *req)
{
}

bool tty::onWriteDirect(request_dev_t *req)
{
    writeString((const char*) req->params.dev_write_direct.buffer,
        req->params.dev_write_direct.length);
    return true;
}

device_t *TtyAddDevice(driver_t *drv, const wchar_t *name, device_config_t *cfg)
{
    tty *tty;
    wchar_t ch;
    
    ch = name[3];
    if (ch >= '0' && ch <= '9')
	tty = consoles + ch - '0';
    else
	tty = consoles + num_consoles;

    tty->driver = drv;

    if (tty > consoles)
	tty->clear();
    if (tty == consoles + 1)
        tty->switchTo();

    SemAcquire(&sem_consoles);
    num_consoles++;
    SemRelease(&sem_consoles);

    return tty;
}

extern "C" void (*__CTOR_LIST__[])();
extern "C" void (*__DTOR_LIST__[])();

extern "C" bool DrvInit(driver_t *drv)
{
    void (**pfunc)() = __CTOR_LIST__;

    /* Cygwin dcrt0.cc sources do this backwards */
    while (*++pfunc)
        ;
    while (--pfunc > __CTOR_LIST__)
        (*pfunc) ();

    drv->add_device = TtyAddDevice;
    consoles[0].driver = drv;
    return true;
}

void TtySwitchConsoles(unsigned n)
{
    consoles[n].switchTo();
}
