// $Id$
#ifndef CONSOLE_H__
#define CONSOLE_H__

#include <gui/Window.h>
#include <os/defs.h>

class Console : public OS::WindowImpl
{
protected:
	bool OnCreate();
	void OnKeyDown(uint32_t key);
    void OnPaint();
	void OnSize(unsigned width, unsigned height);

	void DoEscape(int code, unsigned num, const unsigned *esc);

private:
	lmutex_t m_lmux;
	handle_t m_client;
	unsigned m_width, m_height;
    unsigned m_x, m_y;
	unsigned m_char_width, m_char_height;
    wchar_t *m_buf_chars;
	bool m_cursor_visible;
	colour_t m_fg_colour;
	colour_t m_bg_colour;
	colour_t m_default_fg_colour;
	colour_t m_default_bg_colour;
    unsigned m_save_x, m_save_y;
    unsigned m_escape, m_esc[3], m_esc_param;
	unsigned m_str_x, m_str_y;

	void DrawCursor(bool do_draw);
	void BeginOutput();
	void EndOutput();
	void Scroll();
	void ClearEol();
	void Clear();

public:
	Console(handle_t client);
	~Console();
    void WriteString(const wchar_t *str, size_t length);
};

#endif
