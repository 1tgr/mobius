/* $Id$ */

#ifndef COMMON_H_
#define COMMON_H_

#if 0
/*#ifdef WIN32*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdlib.h>
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

typedef HANDLE handle_t;

typedef BYTE uint8_t;
typedef WORD uint16_t;
typedef DWORD uint32_t;
typedef unsigned __int64 uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef long int32_t;
typedef __int64 int64_t;
typedef DWORD status_t;

#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#define WmgrGetThreadId()   GetCurrentThreadId()

#else

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <os/syscall.h>
#include <os/rtl.h>
#include <os/port.h>
#include <os/defs.h>

#define WmgrGetThreadId()   ThrGetThreadInfo()->id

#endif

#define LIST_ADD(list, item) \
    if (list##_last != NULL) \
        list##_last->next = item; \
    item->prev = list##_last; \
    item->next = NULL; \
    list##_last = item; \
    if (list##_first == NULL) \
            list##_first = item;

#define LIST_REMOVE(list, item) \
    if (item->next != NULL) \
        item->next->prev = item->prev; \
    if (item->prev != NULL) \
        item->prev->next = item->next; \
    if (list##_first == item) \
        list##_first = item->next; \
    if (list##_last == item) \
        list##_last = item->prev; \
    item->next = item->prev = NULL;

#endif
