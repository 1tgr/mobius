/* $Id: strdup.c,v 1.1.1.1 2002/12/21 09:50:18 pavlovskii Exp $ */

#include <string.h>
#include <stdlib.h>

#undef _strdup
char *_strdup(const char* str)
{
    char *ret;
    
    if (str == NULL)
	str = "";

    ret = malloc(strlen(str) + 1);

    if (!ret)
	return NULL;

    strcpy(ret, str);
    return ret;
}
