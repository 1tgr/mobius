/* $Id: srandom.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <unistd.h>
#include <stdlib.h>

void srandom(unsigned seed)
{
    srand(seed);
}
