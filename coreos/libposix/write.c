/* $Id: write.c,v 1.1.1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <unistd.h>
#include <os/rtl.h>

ssize_t	write(int fd, const void *buf, size_t nbyte)
{
	size_t bytes;

	if (!FsWrite(fd, buf, nbyte, &bytes))
		return -1;

	return bytes;
}
