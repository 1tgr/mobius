/* $Id: stddef.h,v 1.1.1.1 2002/12/31 01:26:20 pavlovskii Exp $ */
#ifndef __STDDEF_H
#define __STDDEF_H

#include <sys/types.h>

#define NULL 0
#define offsetof(s,m)		(size_t)&(((s *)0)->m)
#define _countof(a)			(sizeof(a) / sizeof((a)[0]))

#ifndef min
#define min(a, b)			((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b)			((a) > (b) ? (a) : (b))
#endif

#endif
