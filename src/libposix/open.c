/* $Id: open.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <io.h>
#include <fcntl.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <stdlib.h>

wchar_t *_towc(const char *mb);

int open(const char *path, int oflag, ...)
{
    wchar_t *wc = _towc(path);
    uint32_t mode;
    handle_t (*fn)(const wchar_t*, uint32_t);
    handle_t fd;

    if (oflag & O_CREAT)
	fn = FsCreate;
    else
	fn = FsOpen;

    if (oflag & O_RDWR)
	mode = FILE_READ | FILE_WRITE;
    else if (oflag & O_WRONLY)
	mode = FILE_WRITE;
    else
	mode = FILE_READ;
    
    fd = fn(wc, mode);
    free(wc);
    return fd;
}
