/* $Id: read.c,v 1.2 2002/03/07 16:26:04 pavlovskii Exp $ */

#include <unistd.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <errno.h>

ssize_t	_read(int fd, void *buf, size_t nbyte)
{
	fileop_t op;

	op.event = (handle_t) fd;
	if (!FsRead((handle_t) fd, buf, nbyte, &op))
	{
		errno = op.result;
		return op.bytes;
	}

	if (op.result == SIOPENDING)
		ThrWaitHandle(op.event);

	if (op.result > 0)
		errno = op.result;
	
	return op.bytes;
}