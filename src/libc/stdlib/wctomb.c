#include <stdlib.h>

int wctomb(char *_s, wchar_t _wchar)
{
	return wcstombs(_s, _wchar, 1);
}