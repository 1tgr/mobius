/* $Id: htonl.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <arpa/inet.h>

uint32_t htonl(uint32_t hostlong)
{
    return ((hostlong & 0xff000000) >> 24) | 
           ((hostlong & 0x00ff0000) >> 8) | 
           ((hostlong & 0x0000ff00) << 8) | 
           ((hostlong & 0x000000ff) << 24);
}
