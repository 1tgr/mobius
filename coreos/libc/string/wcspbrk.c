/* $Id: wcspbrk.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <string.h>

wchar_t *
wcspbrk(const wchar_t *s1, const wchar_t *s2)
{
	const wchar_t *scanp;
	int c, sc;

	while ((c = *s1++) != 0)
	{
		for (scanp = s2; (sc = *scanp++) != 0;)
			if (sc == c)
				return (wchar_t*) s1 - 1;
	}
	return 0;
}
