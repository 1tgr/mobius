/* $Id: lseek.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <unistd.h>
#include <os/syscall.h>

off_t lseek(int fd, off_t offset, int whence)
{
	return FsSeek((handle_t) fd, offset, whence);
}
