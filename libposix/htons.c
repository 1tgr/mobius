/* $Id: htons.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <arpa/inet.h>

uint16_t htons(uint16_t hostshort)
{
    return ((hostshort & 0xff00) >> 8) | 
           ((hostshort & 0x00ff) << 8);
}
