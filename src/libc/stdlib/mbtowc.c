/* $Id: mbtowc.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <stdlib.h>

int mbtowc(wchar_t *_pwc, const char *_s, size_t _n)
{
	return mbstowcs(_pwc, _s, _n);
}