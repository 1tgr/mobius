/* $Id: ntohs.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <arpa/inet.h>

uint16_t ntohs(uint16_t netshort)
{
    return ((netshort & 0xff00) >> 8) | 
           ((netshort & 0x00ff) << 8);
}
