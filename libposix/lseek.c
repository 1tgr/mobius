/* $Id: lseek.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <os/syscall.h>

off_t lseek(int fd, off_t offset, int whence)
{
	return FsSeek((handle_t) fd, offset, whence);
}
