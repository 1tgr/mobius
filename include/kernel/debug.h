#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

#ifdef DEBUG
#include <stdio.h>
#define TRACE0(f)		wprintf(L##f)
#define TRACE1(f, a)	wprintf(L##f, a)
#define TRACE2(f, a, b)	wprintf(L##f, a, b)
#define TRACE3(f, a, b, c)	wprintf(L##f, a, b, c)
#define TRACE4(f, a, b, c, d)	wprintf(L##f, a, b, c, d)
#else
#define TRACE0(f)
#define TRACE1(f, a)
#define TRACE2(f, a, b)
#define TRACE3(f, a, b, c)
#define TRACE4(f, a, b, c, d)
#endif

#endif