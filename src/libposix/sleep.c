/* $Id: sleep.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <unistd.h>
#include <os/syscall.h>

unsigned sleep(unsigned seconds)
{
    ThrSleep(seconds * 1000);
    return 0;
}
