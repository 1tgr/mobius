/* $Id: read.c,v 1.1.1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <os/rtl.h>

ssize_t	read(int fd, void *buf, size_t nbyte)
{
	size_t bytes;

	if (!FsRead(fd, buf, nbyte, &bytes))
		return -1;

	return bytes;
}
