/* $Id: random.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <stdlib.h>

long random(void)
{
    return rand();
}
