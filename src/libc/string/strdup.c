/* $Id: strdup.c,v 1.1 2002/05/12 00:30:34 pavlovskii Exp $ */

#include <string.h>
#include <stdlib.h>

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
