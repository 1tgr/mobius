/* $Id: ntohl.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <arpa/inet.h>

uint32_t ntohl(uint32_t netlong)
{
    return ((netlong & 0xff000000) >> 24) | 
           ((netlong & 0x00ff0000) >> 8) | 
           ((netlong & 0x0000ff00) << 8) | 
           ((netlong & 0x000000ff) << 24);
}
