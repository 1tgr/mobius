/* $Id: stddef.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
#ifndef __STDDEF_H
#define __STDDEF_H

#include <sys/types.h>

#define NULL 0
#define offsetof(s,m)		(size_t)&(((s *)0)->m)
#define _countof(a)			(sizeof(a) / sizeof((a)[0]))

#define min(a, b)			((a) < (b) ? (a) : (b))
#define max(a, b)			((a) > (b) ? (a) : (b))

#endif