/* $Id: ntohl.c,v 1.1.1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <arpa/inet.h>

uint32_t ntohl(uint32_t netlong)
{
    return ((netlong & 0xff000000) >> 24) | 
           ((netlong & 0x00ff0000) >> 8) | 
           ((netlong & 0x0000ff00) << 8) | 
           ((netlong & 0x000000ff) << 24);
}
