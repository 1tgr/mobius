/* $Id: inet.h,v 1.1 2002/05/05 13:50:37 pavlovskii Exp $ */

#ifndef __POSIX_ARPA_INET_H
#define __POSIX_ARPA_INET_H

#include <sys/types.h>

uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);

#endif
