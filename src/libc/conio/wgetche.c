#include <conio.h>
#include <stdio.h>

//! Waits for a key to be pressed at the console and echoes the key pressed.
/*!
 *	\return	The code of the key pressed. Codes 0-0xFFFF follow the Unicode UCS; 
 *		other codes are defined by the operating system and correspond to 
 *		non-printable keys on the keyboard.
 */
wint_t _wgetche()
{
	wint_t ret = _wgetch();
	putwchar(ret);
	return ret;
}