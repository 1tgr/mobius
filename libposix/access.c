/* $Id: access.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <stdlib.h>
#include <os/syscall.h>

wchar_t *_towc(const char *mb);

int access(const char *fn, int flags)
{
    wchar_t *wc = _towc(fn);
    int ret;

    if (FsQueryFile(wc, 0, NULL, 0))
	ret = 0;
    else
	ret = -1;
    
    free(wc);
    return ret;
}
