#include <wchar.h>

extern const wchar_info_t unicode[];

//! Returns true if the specified character is upper-case.
/*
 *	Use towupper to obtain the upper-case version of a character.
 *	Not all character sets distinguish between upper and lower case.
 *	\param	c	Specifies the character to test.
 */
int towupper(wint_t c)
{
	return unicode[c].upper;
}