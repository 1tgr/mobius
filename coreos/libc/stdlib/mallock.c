/*
 * $History: mallock.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 14:14
 * Updated in $/coreos/libc/stdlib
 * Move atexit(__malloc_lock_cleanup) call into main.c, to solve
 * chicken/egg problem with malloc lock
 * Added history block
 */

#include <malloc.h>
#include <stdlib.h>
#include <os/rtl.h>
#include <os/defs.h>
#include <os/syscall.h>
#include <libc/local.h>
#include "init.h"

static lmutex_t lmux_malloc;

int __malloc_lock(int lock_unlock, unsigned long *lock)
{
    if (lock_unlock)
        LmuxAcquire(&lmux_malloc);
    else
        LmuxRelease(&lmux_malloc);
    return 0;
}


void __malloc_lock_cleanup(void)
{
    LmuxDelete(&lmux_malloc);
}


void __malloc_lock_init(void)
{
    LmuxInit(&lmux_malloc);
}
