/* $Id: lseek.c,v 1.2 2002/03/07 16:26:04 pavlovskii Exp $ */

#include <unistd.h>
#include <os/syscall.h>

off_t lseek(int fd, off_t offset, int whence)
{
	return FsSeek((handle_t) fd, offset, whence);
}
