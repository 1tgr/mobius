/* $Id: random.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <unistd.h>
#include <stdlib.h>

long random(void)
{
    return rand();
}
