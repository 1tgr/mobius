#include <stdlib.h>

int mbtowc(wchar_t *_pwc, const char *_s, size_t _n)
{
	return mbstowcs(_pwc, _s, _n);
}