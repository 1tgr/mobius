/* $Id: htonl.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <arpa/inet.h>

uint32_t htonl(uint32_t hostlong)
{
    return ((hostlong & 0xff000000) >> 24) | 
           ((hostlong & 0x00ff0000) >> 8) | 
           ((hostlong & 0x0000ff00) << 8) | 
           ((hostlong & 0x000000ff) << 24);
}
