/* $Id: opendir.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <unistd.h>
#include <errno.h>
#include <dirent.h>

DIR *opendir(const char *dirname)
{
    errno = ENOTIMPL;
    return NULL;
}
