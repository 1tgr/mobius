#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/arch.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef struct tty_t tty_t;
struct tty_t : public device_t
{
	bool request(request_t *req);
	bool isr(uint8_t irq);

	void TtyUpdateCursor();
	void TtySwitchTo();
	void TtyScroll();
	void TtyFlush();
	void TtyClear();
	void TtyDoEscape(char code, unsigned num, const unsigned *esc);
	void TtyWriteString(const wchar_t *str, size_t count);

	uint16_t *buf_top;
	uint16_t attribs;
	unsigned x, y, width, height, save_x, save_y;
	unsigned escape, esc[3], esc_param;
	const wchar_t *writebuf;
	size_t writeptr;
};

unsigned num_consoles = 1;
semaphore_t sem_consoles;
tty_t consoles[12];
unsigned cur_console;

/* 7-bit ASCII maps to bottom 128 characters of UCS */
typedef struct
{
	wchar_t ucs;
	unsigned char ascii;
} asciimap_t;

asciimap_t ascii437_to_ucs[128] =
{
	{ 0x0092,	0x9f },
	{ 0x00A0,	0xff },
	{ 0x00A1,	0xad },
	{ 0x00A2,	0xbd },
	{ 0x00A3,	0x9c },
	{ 0x00A4,	0xcf },
	{ 0x00A5,	0xbe },
	{ 0x00A6,	0xdd },
	{ 0x00A7,	0xf5 },
	{ 0x00A8,	0xf9 },
	{ 0x00A9,	0xb8 },
	{ 0x00AA,	0xa6 },
	{ 0x00AB,	0xae },
	{ 0x00AC,	0xaa },
	{ 0x00AD,	0xf0 },
	{ 0x00AE,	0xa9 },
	{ 0x00AF,	0xee },
	{ 0x00B0,	0xf8 },
	{ 0x00B1,	0xf1 },
	{ 0x00B2,	0xfd },
	{ 0x00B3,	0xfc },
	{ 0x00B4,	0xef },
	{ 0x00B5,	0xe6 },
	{ 0x00B6,	0xf4 },
	{ 0x00B7,	0xfa },
	{ 0x00B8,	0xf7 },
	{ 0x00B9,	0xfb },
	{ 0x00BA,	0xa7 },
	{ 0x00BB,	0xaf },
	{ 0x00BC,	0xac },
	{ 0x00BD,	0xab },
	{ 0x00BE,	0xf3 },
	{ 0x00BF,	0xa8 },
	{ 0x00C0,	0xb7 },
	{ 0x00C1,	0xb5 },
	{ 0x00C2,	0xb6 },
	{ 0x00C3,	0xc7 },
	{ 0x00C4,	0x8e },
	{ 0x00C5,	0x8f },
	{ 0x00C6,	0x92 },
	{ 0x00C7,	0x80 },
	{ 0x00C8,	0xd4 },
	{ 0x00C9,	0x90 },
	{ 0x00CA,	0xd2 },
	{ 0x00CB,	0xd3 },
	{ 0x00CC,	0xde },
	{ 0x00CD,	0xd6 },
	{ 0x00CE,	0xd7 },
	{ 0x00CF,	0xd8 },
	{ 0x00D0,	0xd1 },
	{ 0x00D1,	0xa5 },
	{ 0x00D2,	0xe3 },
	{ 0x00D3,	0xe0 },
	{ 0x00D4,	0xe2 },
	{ 0x00D5,	0xe5 },
	{ 0x00D6,	0x99 },
	{ 0x00D7,	0x9e },
	{ 0x00D8,	0x9d },
	{ 0x00D9,	0xeb },
	{ 0x00DA,	0xe9 },
	{ 0x00DB,	0xea },
	{ 0x00DC,	0x9a },
	{ 0x00DD,	0xed },
	{ 0x00DE,	0xe8 },
	{ 0x00DF,	0xe1 },
	{ 0x00E0,	0x85 },
	{ 0x00E0,	0xa1 },
	{ 0x00E1,	0xa0 },
	{ 0x00E2,	0x83 },
	{ 0x00E3,	0xc6 },
	{ 0x00E4,	0x84 },
	{ 0x00E5,	0x86 },
	{ 0x00E6,	0x91 },
	{ 0x00E7,	0x87 },
	{ 0x00E8,	0x8a },
	{ 0x00E9,	0x82 },
	{ 0x00EA,	0x88 },
	{ 0x00EB,	0x89 },
	{ 0x00EC,	0x8d },
	{ 0x00EE,	0x8c },
	{ 0x00EF,	0x8b },
	{ 0x00F0,	0xd0 },
	{ 0x00F1,	0xa4 },
	{ 0x00F2,	0x95 },
	{ 0x00F3,	0xa2 },
	{ 0x00F4,	0x93 },
	{ 0x00F5,	0xe4 },
	{ 0x00F6,	0x94 },
	{ 0x00F7,	0xf6 },
	{ 0x00F8,	0x9b },
	{ 0x00F9,	0x97 },
	{ 0x00FA,	0xa3 },
	{ 0x00FB,	0x96 },
	{ 0x00FC,	0x81 },
	{ 0x00FD,	0xec },
	{ 0x00FE,	0xe7 },
	{ 0x00FF,	0x98 },
	{ 0x0131,	0xd5 },
	{ 0x2017,	0xf2 },
	{ 0x20A0,	0xfe },
	{ 0x2500,	0xc4 },
	{ 0x2502,	0xb3 },
	{ 0x250C,	0xda },
	{ 0x2510,	0xbf },
	{ 0x2514,	0xc0 },
	{ 0x2518,	0xd9 },
	{ 0x251C,	0xc3 },
	{ 0x2524,	0xb4 },
	{ 0x252C,	0xc2 },
	{ 0x2534,	0xc1 },
	{ 0x253C,	0xc5 },
	{ 0x2550,	0xcd },
	{ 0x2551,	0xba },
	{ 0x2554,	0xc9 },
	{ 0x2557,	0xbb },
	{ 0x255A,	0xc8 },
	{ 0x255D,	0xbc },
	{ 0x2560,	0xcc },
	{ 0x2563,	0xb9 },
	{ 0x2566,	0xcb },
	{ 0x2569,	0xca },
	{ 0x256C,	0xce },
	{ 0x2580,	0xdf },
	{ 0x2584,	0xdc },
	{ 0x2588,	0xdb },
	{ 0x2591,	0xb0 },
	{ 0x2592,	0xb1 },
	{ 0x2593,	0xb2 },
};

void *
bsearch(const void *key, const void *base0, size_t nelem,
	size_t size, int (*cmp)(const void *ck, const void *ce))
{
	char *base = (char*) base0;
	int lim, cmpval;
	void *p;

	for (lim = nelem; lim != 0; lim >>= 1)
	{
		p = base + (lim >> 1) * size;
		cmpval = (*cmp)(key, p);
		if (cmpval == 0)
			return p;

		if (cmpval > 0)
		{				/* key > p: move right */
			base = (char *)p + size;
			lim--;
		} /*,lse move left */
	}
	return 0;
}

static int asciimap_search_wchar_t(const void* ck, const void* ce)
{
	const wchar_t* key = (const wchar_t*) ck;
	const asciimap_t* elem = (const asciimap_t*) ce;
	return *key - elem->ucs;
}

size_t wcsto437(char *mbstr, const wchar_t *wcstr, size_t count)
{
	asciimap_t* map;
	size_t i;

	for (i = 0; *wcstr && i < count; i++)
	{
		if (*wcstr < 128)
			*mbstr = (unsigned char) *wcstr;
		else
		{
			map = (asciimap_t*) bsearch(wcstr, ascii437_to_ucs, 
				_countof(ascii437_to_ucs), 
				sizeof(asciimap_t), asciimap_search_wchar_t);

			if (map)
				*mbstr = map->ascii;
			else
				*mbstr = '?';
		}

		wcstr++;
		mbstr++;
	}

	*mbstr = 0;
	return i;
}

#define _crtc_base_adr	0x3C0

/* I/O addresses for VGA registers */
#define	VGA_MISC_READ	0x3CC
#define	VGA_GC_INDEX	0x3CE
#define	VGA_GC_DATA		0x3CF
#define	VGA_CRTC_INDEX	0x14 /* relative to 0x3C0 or 0x3A0 */
#define	VGA_CRTC_DATA	0x15 /* relative to 0x3C0 or 0x3A0 */

void memset16(uint16_t *ptr, uint16_t c, size_t count)
{
	while (count-- > 0)
		*ptr++ = c;
}

void tty_t::TtyUpdateCursor()
{
	uint16_t addr;
	/* move cursor */

	if ((unsigned) (this - consoles) == cur_console)
	{
		addr = y * width + x;
		out(_crtc_base_adr + VGA_CRTC_INDEX, 14);
		out(_crtc_base_adr + VGA_CRTC_DATA, addr >> 8);
		out(_crtc_base_adr + VGA_CRTC_INDEX, 15);
		out(_crtc_base_adr + VGA_CRTC_DATA, addr);
	}
}

void tty_t::TtySwitchTo()
{
	uint16_t addr;
	addr = (uint16_t) (addr_t) (buf_top - (uint16_t*) PHYSICAL(0xb8000));
	
	/* set base address */
	out(_crtc_base_adr + VGA_CRTC_INDEX, 12);
	out(_crtc_base_adr + VGA_CRTC_DATA, addr >> 8);
	out(_crtc_base_adr + VGA_CRTC_INDEX, 13);
	out(_crtc_base_adr + VGA_CRTC_DATA, addr);
	cur_console = this - consoles;

	TtyUpdateCursor();
}

extern "C" void TtySwitchConsoles(unsigned n)
{
	consoles[n].TtySwitchTo();
}

void tty_t::TtyScroll()
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

	TtyUpdateCursor();
}

void tty_t::TtyFlush()
{
	char mb[81], *ch;
	size_t count;
	uint16_t *out;

	count = wcsto437(mb, writebuf, min(writeptr, _countof(mb)));
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
			TtyScroll();
			out = buf_top + width * y + x;
		}
	}

	writebuf += writeptr;
	writeptr = 0;
}

void tty_t::TtyClear()
{
	memset16(buf_top, attribs | ' ', width * height);
	x = 0;
	y = 0;
	TtyUpdateCursor();
}

void tty_t::TtyDoEscape(char code, unsigned num, const unsigned *esc)
{
	unsigned i;
	static const char ansi_to_vga[] =
	{
		0, 4, 2, 6, 1, 5, 3, 7
	};

	switch (code)
	{
	case 'H':	/* ESC[PL;PcH - cursor position */
	case 'f':	/* ESC[PL;Pcf - cursor position */
		if (num > 0)
			y = min(esc[0], height - 1);
		if (num > 1)
			x = min(esc[1], width - 1);
		TtyUpdateCursor();
		break;
	
	case 'A':	/* ESC[PnA - cursor up */
		if (num > 0 && y >= esc[0])
		{
			y -= esc[0];
			TtyUpdateCursor();
		}
		break;
	
	case 'B':	/* ESC[PnB - cursor down */
		if (num > 0 && y < height + esc[0])
		{
			y += esc[0];
			TtyUpdateCursor();
		}
		break;

	case 'C':	/* ESC[PnC - cursor forward */
		if (num > 0 && x < width + esc[0])
		{
			x += esc[0];
			TtyUpdateCursor();
		}
		break;
	
	case 'D':	/* ESC[PnD - cursor backward */
		if (num > 0 && y >= esc[0])
		{
			x -= esc[0];
			TtyUpdateCursor();
		}
		break;

	case 's':	/* ESC[s - save cursor position */
		save_x = x;
		save_y = y;
		break;

	case 'u':	/* ESC[u - restore cursor position */
		x = save_x;
		y = save_y;
		TtyUpdateCursor();
		break;

	case 'J':	/* ESC[2J - clear screen */
		TtyClear();
		break;

	case 'K':	/* ESC[K - erase line */
		memset16(buf_top + width * y + x,
			attribs | ' ',
			width - x);
		break;

	case 'm':	/* ESC[Ps;...Psm - set graphics mode */
		for (i = 0; i < num; i++)
		{
			if (esc[i] < 8)
			{
				switch (esc[i])
				{
				case 0:	/* all attributes off */
					attribs &= 0x7700;
					break;
				case 1:	/* bold (high-intensity) */
					attribs |= 0x0800;
					break;
				case 4:	/* underscore */
					break;
				case 5:	/* blink */
					attribs |= 0x8000;
					break;
				case 7:	/* reverse video */
					attribs = (attribs & 0x0f00) >> 8 |
						(attribs & 0x0f00) << 8;
					break;
				case 8:	/* concealed */
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

	case 'h':	/* ESC[=psh - set mode */
	case 'l':	/* ESC[=Psl - reset mode */
	case 'p':	/* ESC[code;string;...p - set keyboard strings */
		/* not supported */
		break;
	}
}

void tty_t::TtyWriteString(const wchar_t *str, size_t count)
{
	writebuf = str;
	writeptr = 0;
	while (count > 0)
	{
		switch (escape)
		{
		case 0:	/* normal character */
			switch (*str)
			{
			case 27:
				TtyFlush();
				escape++;
				break;
			case '\t':
				TtyFlush();
				writebuf = str + 1;
				x = (x + 4) & ~3;
				if (x >= width)
				{
					x = 0;
					y++;
					TtyScroll();
				}
				TtyUpdateCursor();
				break;
			case '\n':
				TtyFlush();
				writebuf = str + 1;
				x = 0;
				y++;
				TtyScroll();
				TtyUpdateCursor();
				break;
			default:
				writeptr++;
				if (writeptr >= 80)
					TtyFlush();
			}
			break;

		case 1:	/* after ESC */
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

		case 2:	/* after ESC[ */
		case 3:	/* after ESC[n; */
		case 4:	/* after ESC[n;n; */
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

		case 10:	/* after all parameters */
			/*wprintf(L"esc: %c: %u/%u/%u\n", 
				*str, 
				esc[0],
				esc[1],
				esc[2]);*/
			TtyDoEscape((char) *str, esc_param + 1, esc);
			escape = 0;
			writebuf = str + 1;
			writeptr = 0;
			break;
		}

		str++;
		count--;
	}

	TtyFlush();
}

bool tty_t::isr(uint8_t irq)
{
	uint8_t scan;
	scan = in(0x60);
	if (scan >= 0x3b && scan < 0x3b + 12)
	{
		consoles[scan - 0x3b].TtySwitchTo();
		return true;
	}
	else
		return false;
}

bool tty_t::request(request_t* req)
{
	request_dev_t *req_dev = (request_dev_t*) req;
	
	switch (req->code)
	{
	case DEV_REMOVE:
		return true;

	case DEV_WRITE:
		TtyWriteString((const wchar_t*) req_dev->params.dev_write.buffer, 
			req_dev->params.dev_write.length / sizeof(wchar_t));
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

device_t *TtyAddDevice(driver_t *drv, const wchar_t *name, device_config_t *cfg)
{
	tty_t *tty;
	
	/*tty = malloc(sizeof(device_t));*/
	tty = consoles + num_consoles;
	//memset(tty, 0, sizeof(tty));
	//dev.vtbl = &tty_vtbl;
	//dev.driver = drv;
	tty->driver = drv;

	SemAcquire(&sem_consoles);
	tty->buf_top = (uint16_t*) PHYSICAL(0xb8000) + 80 * 25 * num_consoles;
	num_consoles++;
	SemRelease(&sem_consoles);

	tty->attribs = 0x1700;
	tty->width = 80;
	tty->height = 25;
	tty->escape = 0;

	tty->TtyClear();
	/*TtySwitchTo(tty);*/
	return tty;
}

extern "C" bool DrvInit(driver_t *drv)
{
	drv->add_device = TtyAddDevice;

	consoles[0].driver = drv;
	consoles[0].buf_top = (uint16_t*) PHYSICAL(0xb8000);
	consoles[0].attribs = 0x1700;
	consoles[0].x = 0;
	consoles[0].y = 0;
	consoles[0].width = 80;
	consoles[0].height = 25;
	consoles[0].escape = 0;

	/*DevRegisterIrq(1, &consoles[0].dev);*/
	return true;
}
