/* $Id: time.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

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
