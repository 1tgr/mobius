/* $Id: stat.c,v 1.1.1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <sys/stat.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <stdlib.h>

wchar_t *_towc(const char *mb);

int stat(const char *path, struct stat *buf)
{
    wchar_t *wc;
    dirent_standard_t di;

    wc = _towc(path);
    if (wc == NULL)
        return -1;

    if (!FsQueryFile(wc, FILE_QUERY_STANDARD, &di, sizeof(di)))
    {
        free(wc);
        return -1;
    }

    free(wc);
    buf->st_atime = 0;
    buf->st_ctime = 0;
    buf->st_dev = 0;
    buf->st_gid = 0;
    buf->st_ino = 0;
    buf->st_mode = 0;
    buf->st_mtime = 0;
    buf->st_nlink = 1;
    buf->st_size = di.length;
    buf->st_blksize = 0;
    buf->st_uid = 0;
    return 0;
}
