// $Id$
#include "console.h"

#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

#include <gui/Graphics.h>
#include <os/rtl.h>

using namespace OS;

wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t count)
{
	wchar_t *ret;

	ret = dest;
	while (count-- > 0)
		*dest++ = *src++;

	return ret;
}


wchar_t *wmemset(wchar_t *dest, wchar_t c, size_t count)
{
	wchar_t *ret;

	ret = dest;
	while (count-- > 0)
		*dest++ = c;

	return ret;
}


Console::Console(handle_t client)
{
	Painter p(this);
	Point size;

	LmuxInit(&m_lmux);
	m_client = client;
	m_width = 0;
	m_height = 0;
	m_x = 0;
	m_y = 0;

	size = p.GetTextSize(L"X");
	m_char_width = size.x;
	m_char_height = size.y;

	m_buf_chars = NULL;
	m_cursor_visible = false;
	m_fg_colour = m_default_fg_colour = 0xc0c0c0;
	m_bg_colour = m_default_bg_colour = 0x000000;
	m_save_x = 0;
	m_save_y = 0;
	m_escape = 0;
}


Console::~Console()
{
	free(m_buf_chars);
	LmuxDelete(&m_lmux);
}


bool Console::OnCreate()
{
	OnSize(m_position.Width(), m_position.Height());
	return true;
}


void Console::OnKeyDown(uint32_t key)
{
	FsWrite(m_client, &key, 0, sizeof(key), NULL);
}


void Console::OnPaint()
{
    Painter p(this);
	int i;
	Rect rect_paint, rect;
	Rect box_text;
	wchar_t *ptr;

    p.SetFillColour(0);
    p.SetPenColour(0xffffff);

	rect_paint = p.m_invalid_rect;
	rect_paint.Offset(-m_position.left, -m_position.top);

	box_text.left = rect_paint.left / m_char_width;
	box_text.top = rect_paint.top / m_char_height;
	box_text.right = (rect_paint.right + m_char_width) / m_char_width;
	box_text.bottom = (rect_paint.bottom + m_char_height) / m_char_height;

	rect = box_text;
	rect.left *= m_char_width;
	rect.top *= m_char_height;
	rect.right *= m_char_width;
	rect.bottom *= m_char_height;
	rect.Offset(m_position.left, m_position.top);

	//p.SetFillColour(MAKE_COLOUR(rand() % 255, rand() % 255, rand() % 255));
	//p.FillRect(rect);

	ptr = m_buf_chars + box_text.top * m_width + box_text.left;
	for (i = box_text.top; i < box_text.bottom; i++)
	{
		rect.bottom = rect.top + m_char_height;
		p.DrawText(rect, ptr, box_text.Width());
		ptr += m_width;
		rect.top = rect.bottom;
	}
}


void Console::OnSize(unsigned width, unsigned height)
{
	size_t old_length, new_length;
	unsigned i;

	old_length = m_width * m_height;
	m_width = width / m_char_width;
	m_height = height / m_char_height;
	new_length = m_width * m_height;

	m_buf_chars = (wchar_t*) realloc(m_buf_chars, 
		new_length * sizeof(*m_buf_chars));
	for (i = old_length; i < new_length; i++)
		m_buf_chars[i] = ' ';
}


void Console::DrawCursor(bool do_draw)
{
}


void Console::Scroll()
{
	unsigned lines, y;
	wchar_t *ptr;
	Rect rect(0, 0, m_width * m_char_width, m_height * m_char_height);

	rect.Offset(m_position.left, m_position.top);
	if (m_y > m_height - 1)
	{
		lines = m_y - m_height + 1;
		if (lines > m_height)
			lines = m_height;

		ptr = m_buf_chars;
		y = 0;

		while (y < m_height - lines)
		{
			wmemcpy(ptr, ptr + m_width, m_width);
			ptr += m_width;
			y++;
		}

		while (y < m_height)
		{
			wmemset(ptr, ' ', m_width);
			ptr += m_width;
			y++;
		}

		m_y = m_height - 1;

		Invalidate(rect);
	}
}


void Console::Clear()
{
	wchar_t *ptr;
	unsigned y;

	Rect rect(0, 0, m_width * m_char_width, m_height * m_char_height);
	rect.Offset(m_position.left, m_position.top);

	ptr = m_buf_chars;
	y = 0;

	while (y < m_height)
	{
		wmemset(ptr, ' ', m_width);
		ptr += m_width;
		y++;
	}

	Invalidate(rect);
	m_x = m_y = 0;
}


void Console::ClearEol()
{
	Rect rect(m_x * m_char_width, m_y * m_char_height,
		m_width * m_char_width, (m_y + 1) * m_char_height);
	rect.Offset(m_position.left, m_position.top);

	wmemset(m_buf_chars + m_y * m_width + m_x, ' ', m_width - m_x);
	Invalidate(rect);
}


void Console::BeginOutput()
{
	m_str_x = m_x;
	m_str_y = m_y;
}


void Console::EndOutput()
{
	assert(m_x >= m_str_x);
	assert(m_y == m_str_y);

	if (m_x != m_str_x)
	{
		Rect rect;
		rect.left = m_position.left + m_str_x * m_char_width;
		rect.top = m_position.top + m_str_y * m_char_height;
		rect.right = m_position.left + m_x * m_char_width;
		rect.bottom = m_position.top + (m_y + 1) * m_char_height;
		Invalidate(rect);
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


void Console::DoEscape(int code, unsigned num, const unsigned *esc)
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
            m_y = min(esc[0], m_height - 1);
        if (num > 1)
            m_x = min(esc[1], m_width - 1);
        break;
    
    case 'A':    /* ESC[PnA - cursor up */
        if (num > 0 && m_y >= esc[0])
            m_y -= esc[0];
        break;
    
    case 'B':    /* ESC[PnB - cursor down */
        if (num > 0 && m_y < m_height + esc[0])
            m_y += esc[0];
        break;

    case 'C':    /* ESC[PnC - cursor forward */
        if (num > 0 && m_x < m_width + esc[0])
            m_x += esc[0];
        break;
    
    case 'D':    /* ESC[PnD - cursor backward */
        if (num > 0 && m_x >= esc[0])
            m_x -= esc[0];
        break;

    case 's':    /* ESC[s - save cursor position */
        m_save_x = m_x;
        m_save_y = m_y;
        break;

    case 'u':    /* ESC[u - restore cursor position */
        m_x = m_save_x;
        m_y = m_save_y;
        break;

    case 'J':    /* ESC[2J - clear screen */
        Clear();
        break;

    case 'K':    /* ESC[K - erase line */
		ClearEol();
        break;

    case 'm':    /* ESC[Ps;...Psm - set graphics mode */
        if (num == 0)
		{
            m_fg_colour = m_default_fg_colour;
			m_bg_colour = m_default_bg_colour;
		}
        else for (i = 0; i < num; i++)
        {
            if (esc[i] <= 8)
            {
                switch (esc[i])
                {
                case 0:    /* all attributes off */
					m_fg_colour = m_default_fg_colour;
					m_bg_colour = m_default_bg_colour;
                    break;
                case 1:    /* bold (high-intensity) */
                    m_fg_colour = ConColourLighter(m_default_fg_colour);
                    break;
                case 4:    /* underscore */
                    break;
                case 5:    /* blink */
                    m_bg_colour = ConColourLighter(m_default_bg_colour);
                    break;
                case 7:    /* reverse video */
                    temp_colour = m_bg_colour;
					m_bg_colour = m_fg_colour;
					m_fg_colour = temp_colour;
                    break;
                case 8:    /* concealed */
                    m_fg_colour = m_bg_colour;
                    break;
                }
            }
            else if (esc[i] >= 30 && esc[i] <= 37)
                /* foreground */
                m_default_fg_colour = m_fg_colour = ansi_to_colour[esc[i] - 30];
            else if (esc[i] >= 40 && esc[i] <= 47)
                /* background */
                m_default_bg_colour = m_bg_colour = ansi_to_colour[esc[i] - 40];
        }
        break;

    case 'h':	/* ESC[=psh - set mode */
    case 'l':	/* ESC[=Psl - reset mode */
    case 'p':	/* ESC[code;string;...p - set keyboard strings */
        /* not supported */
        break;

	case 'n':	/* ESC[Pnn - DSR - DEVICE STATUS REPORT */
		switch (esc[0])
		{
		case 6:
			/* send CPR - ACTIVE POSITION REPORT */
			FsWrite(m_client, buf, 0, 
				sprintf(buf, "\x1b[%u;%uR", m_y, m_x),
				NULL);
			break;
		}
		break;
    }
}


void Console::WriteString(const wchar_t *str, size_t length)
{
	wchar_t wc;
	bool cursor_was_visible;

	//LmuxAcquire(&m_lmux);

	cursor_was_visible = m_cursor_visible;
	if (cursor_was_visible)
		DrawCursor(false);

	BeginOutput();

	while (length > 0)
	{
		wc = *str;

		switch (m_escape)
		{
		case 0:    /* normal character */
			switch (wc)
			{
			case 27:
				EndOutput();
				m_escape++;
				break;
			case '\t':
				EndOutput();
				m_x = (m_x + 4) & ~3;
				if (m_x >= m_width)
				{
					m_x = 0;
					m_y++;
					Scroll();
				}
				BeginOutput();
				break;
			case '\n':
				EndOutput();
				m_x = 0;
				m_y++;
				Scroll();
				BeginOutput();
				break;
			case '\r':
				EndOutput();
				m_x = 0;
				BeginOutput();
				break;
			case '\b':
				EndOutput();
				if (m_x > 0)
					m_x--;
				BeginOutput();
				break;
			default:
				m_buf_chars[m_width * m_y + m_x] = wc;
				m_x++;
				break;
			}
			break;

		case 1:    /* after ESC */
			if (wc == '[')
			{
				m_escape++;
				m_esc_param = 0;
				m_esc[0] = 0;
			}
			else
			{
				m_escape = 0;
				BeginOutput();
			}
			break;

		case 2:    /* after ESC[ */
		case 3:    /* after ESC[n; */
		case 4:    /* after ESC[n;n; */
			if (iswdigit(wc))
			{
				m_esc[m_esc_param] = 
					m_esc[m_esc_param] * 10 + wc - '0';
				break;
			}
			else if (wc == ';' &&
				m_esc_param < _countof(m_esc) - 1)
			{
				m_esc_param++;
				m_esc[m_esc_param] = 0;
				m_escape++;
				break;
			}
			else
			{
				m_escape = 10;
				/* fall through */
			}

		case 10:    /* after all parameters */
			DoEscape(wc, m_esc_param + 1, m_esc);
			m_escape = 0;
			BeginOutput();
			break;
		}

		if (m_x >= m_width)
		{
			EndOutput();
			m_x = 0;
			m_y++;
			BeginOutput();
		}

		if (m_y > m_height - 1)
		{
			EndOutput();
			Scroll();
			BeginOutput();
		}

		str++;
		length--;
	}

	EndOutput();

	if (cursor_was_visible)
		DrawCursor(true);

	//LmuxRelease(&m_lmux);
}
