#include <wchar.h>

extern const wchar_info_t unicode[];

//! Returns true if the specified character is lower-case.
/*
 *	Use towlower to obtain the lower-case version of a character.
 *	Not all character sets distinguish between upper and lower case.
 *	\param	c	Specifies the character to test.
 */
int iswlower(wint_t c)
{
	return unicode[c].lower == c;
}