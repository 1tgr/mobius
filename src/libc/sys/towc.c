/* $Id: towc.c,v 1.4 2002/05/12 00:13:20 pavlovskii Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <libc/local.h>

wchar_t *_towc(const char *mb)
{
    size_t len = strlen(mb);
    wchar_t *wc = malloc(sizeof(wchar_t) * (len + 1));

    if (wc == NULL)
	return NULL;

    len = mbstowcs(wc, mb, len);
    if (len == -1)
    {
	free(wc);
	return NULL;
    }
    else
    {
	wc[len] = '\0';
	return wc;
    }
}

