/* $Id: wctomb.c,v 1.2 2001/11/06 01:29:38 pavlovskii Exp $ */

#include <stdlib.h>

int wctomb(char *_s, wchar_t _wchar)
{
	return wcstombs(_s, _wchar, 1);
}