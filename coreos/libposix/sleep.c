/* $Id: sleep.c,v 1.1.1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <os/syscall.h>

unsigned sleep(unsigned seconds)
{
    ThrSleep(seconds * 1000);
    return 0;
}
