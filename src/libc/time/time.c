/* $Id: time.c,v 1.2 2002/05/12 00:13:20 pavlovskii Exp $ */

#include <time.h>
#include <os/syscall.h>

time_t time(time_t *t)
{
    time_t time;

    time = SysUpTime();
    if (t)
        *t = time;

    return time;
}
