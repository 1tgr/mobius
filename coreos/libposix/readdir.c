/* $Id: readdir.c,v 1.1.1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <errno.h>
#include <dirent.h>

struct dirent *readdir(DIR *dirp)
{
    errno = ENOTIMPL;
    return NULL;
}
