#include <wchar.h>

extern const wchar_info_t unicode[];

int towlower(wint_t c)
{
	return unicode[c].lower;
}