/* $Id: opendir.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <errno.h>
#include <dirent.h>

DIR *opendir(const char *dirname)
{
    errno = ENOTIMPL;
    return NULL;
}
