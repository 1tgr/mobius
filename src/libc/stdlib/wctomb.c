/* $Id: wctomb.c,v 1.3 2002/01/15 00:13:06 pavlovskii Exp $ */

#include <stdlib.h>

int wctomb(char *_s, wchar_t _wchar)
{
	return wcstombs(_s, &_wchar, 1);
}