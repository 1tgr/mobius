#include <stdlib.h>
#include <kernel/kernel.h>
#include <printf.h>
#include <wchar.h>
#include <kernel/serial.h>
#include <kernel/sys.h>

#include <kernel/console.h>

//! Determines the destination of kernel debugging messages, written using 
//!		_cputws() or wprintf()
/*!
 *	Initially this is set to modeBlue, so that the first kernel messages are 
 *		written directly to the screen. After the first kernel driver is 
 *		loaded, this is changed to modeConsole, so that the next _cputws()
 *		will try to open the console driver and write to there.
 */
enum PrintfMode printf_mode = modeBlue;

#ifdef BUFFER
static wchar_t printf_buffer[80], *ch;
#endif

int text_width, text_height;

//!	Pointer to a console driver object, used once the console driver is loaded
//IStream* console;
textwin_t spew, checks;
//! Set to true when wprintf is executing
bool wprintf_lock;
textwin_t *wprintf_window = &spew;

console_t *con;

void conSetMode(enum PrintfMode mode)
{
	printf_mode = mode;
	//if (mode == modeBlue && console)
		//console->vtbl->Release(console);
}

void conClear()
{
	con->conScroll(wprintf_window, 0, wprintf_window->top - wprintf_window->bottom);
}

//! Writes a string directly to the kernel console (in "blue-screen" mode)
/*!
 *	\param	str	The string to be written
 *	\return	Zero
 */
int _cputws_blue(textwin_t* win, const wchar_t* str)
{
	while (*str)
	{
		switch (*str)
		{
		case L'\n':
			win->x = win->left;
			win->y++;
			break;

		case L'\r':
			win->x = win->left;
			break;
		
		case L'\t':
			win->x = (win->x + 4) & ~3;
			break;

		case L'\b':
			if (win->x > 0)
				win->x--;
			break;

		default:
			con->conWriteChar(win, win->x, win->y, win->attrib, *str);
			win->x++;
		}

		if (win->x >= win->right)
		{
			win->x = win->left;
			win->y++;
		}

		while (win->y >= win->bottom)
			con->conScroll(win, 0, -1);
		
		//ioWriteByte(NULL, *str);
		str++;
	}

	con->conUpdateCursor(win);
	return 0;
}

void _cputws_check(const wchar_t* str)
{
	_cputws_blue(&checks, str);
	con->conUpdateCursor(&spew);
}

//! Writes a string directly to either the kernel console (in "blue-screen" 
//!		mode) or the console driver
/*!
 *	The value of printf_mode decides where the string will be written.
 *
 *	\param	str	The string to be written
 *	\return	Zero
 */
int _cputws(const wchar_t* str)
{
	return _cputws_blue(&spew, str);
}

wint_t putwchar(wint_t c)
{
	wchar_t str[2] = { c, 0 };
	_cputws(str);
	return c;
}

static bool kprintfhelp(void* pContext, const wchar_t* str, dword len)
{
#ifdef BUFFER
	if (ch + len > printf_buffer + countof(printf_buffer) ||
		*str == L'\r' || *str == L'\n')
	{
		_cputws_blue(wprintf_window, printf_buffer);
		ch = printf_buffer;
	}

	wcscpy(ch, str);
	ch += len;
#else
	_cputws_blue(wprintf_window, str);	
#endif
	return true;
}

int vwprintf(const wchar_t* fmt, va_list ptr)
{
	int ret;

#ifdef BUFFER
	ch = printf_buffer;
#endif

	ret = dowprintf(kprintfhelp, NULL, fmt, ptr);

#ifdef BUFFER
	_cputws_blue(wprintf_window, printf_buffer);
#endif

	return ret;
}

int wprintf(const wchar_t* fmt, ...)
{
	va_list ptr;
	int ret;

	//while (wprintf_lock)
		//;

	wprintf_lock = true;
	va_start(ptr, fmt);
	ret = vwprintf(fmt, ptr);
	va_end(ptr);
	wprintf_lock = false;

	return ret;
}

void conInit(int nMode)
{
	con = vgaInit();
	wprintf(L"Screen is %dx%d\n", text_width, text_height);
}