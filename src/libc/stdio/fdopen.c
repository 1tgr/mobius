/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <libc/stubs.h>
#include <sys/types.h>
#include <stdio.h>
/*#include <posix/fcntl.h>
#include <posix/unistd.h>
#include <posix/io.h>*/
#include <libc/file.h>
#include <libc/local.h>
#include <os/syscall.h>
#include <os/defs.h>

FILE *
fdopen(int fildes, const char *mode)
{
    FILE *f;
    int rw, oflags = 0;
    /*char tbchar;*/

    f = __alloc_file();
    if (f == NULL)
        return NULL;

    rw = (mode[1] == '+') || (mode[1] && (mode[2] == '+'));

    switch (*mode)
    {
    case 'a':
        oflags = FILE_CREATE_OPEN | (rw ? FILE_READ | FILE_WRITE : FILE_WRITE);
        break;
    case 'r':
        oflags = FILE_FORCE_OPEN | (rw ? FILE_READ | FILE_WRITE : FILE_READ);
        break;
    case 'w':
        oflags = FILE_FORCE_CREATE | (rw ? FILE_READ | FILE_WRITE : FILE_READ);
        break;
    default:
        return (NULL);
    }

    /*if (mode[1] == '+')
        tbchar = mode[2];
    else
        tbchar = mode[1];
    if (tbchar == 't')
        oflags |= O_TEXT;
    else if (tbchar == 'b')
        oflags |= O_BINARY;
    else
        oflags |= (_fmode & (O_TEXT|O_BINARY));*/

    if (*mode == 'a')
        FsSeek(fildes, 0, SEEK_END);

    f->_cnt = 0;
    f->_file = fildes;
    f->_bufsiz = 0;
    f->_flag = oflags;

    f->_base = f->_ptr = NULL;

    /*setmode(fildes, oflags & (O_TEXT|O_BINARY));*/

    return f;
}
