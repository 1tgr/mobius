/* $Id: memchr.c,v 1.1 2002/12/21 09:50:17 pavlovskii Exp $ */

/* Copyright (C) 1997 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <string.h>

void *
memchr(const void *s, int c, size_t n)
{
	if (n)
	{
	const char *p = s;
	char cc = c;
	do {
		if (*p == cc)
			return (void*) p;
		p++;
	} while (--n != 0);
	}
	return 0;
}
