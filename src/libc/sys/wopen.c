/* $Id: wopen.c,v 1.1 2002/03/07 16:26:04 pavlovskii Exp $ */

#include <io.h>
#include <fcntl.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <stdlib.h>

int __wopen(const wchar_t *path, int oflag)
{
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
    
    fd = fn(path, mode);
    return fd;
}
