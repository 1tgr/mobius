/* $Id: strings.h,v 1.1 2002/05/05 13:50:37 pavlovskii Exp $ */

#ifndef __POSIX_STRINGS_H
#define __POSIX_STRINGS_H

#include <string.h>

int    ffs(int);

#define bcmp(b1,b2,len)         memcmp((b1), (b2), (size_t)(len))
#define bcopy(b1,b2,len)        (memmove((b2), (b1), (len)), (void) 0)
#define bzero(b,len)            (memset((b), '\0', (len)), (void) 0)
#define index(s,c)              strchr((s), (c))
#define rindex(s,c)             strrchr((s), (c))
#define strcasecmp(s1,s2)       _stricmp((s1), (s2))
#define strncasecmp(s1,s2,n)    _strnicmp((s1), (s2), (n))

#endif
