#include <wchar.h>

extern const wchar_info_t unicode[];

int iswupper(wint_t c)
{
	return unicode[c].upper == c;
}