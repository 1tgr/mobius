#include <os/os.h>
#include <os/console.h>

//! Writes one character to the console.
/*!
 *	Unless the console is in raw mode, the character may be processed 
 *		if it is a control character.
 *	\param	c	The character to be output.
 */
wint_t putwchar(wint_t c)
{
	wchar_t s[2] = { c, 0 };
	_cputws(s);
	return c;
}

//! Writes a string of characters to the console.
/*!
 *	Unless the console is in raw mode, the string may contain control
 *		characters to be processed by the console driver.
 *	\param	str	The string to be output.
 *	\return	Returns 0 (zero) if successful, or a non-zero value otherwise.
 */
int _cputws(const wchar_t* str)
{
	if (conWrite(str))
		return 0;
	else
		return 1;
}
