/* $Id: fs.c,v 1.1 2002/04/10 12:50:18 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/rtl.h>
#include <os/defs.h>
#include <errno.h>

bool FsReadSync(handle_t file, void *buf, size_t bytes, size_t *bytes_read)
{
    fileop_t op;

    op.event = file;
    if (!FsRead(file, buf, bytes, &op))
    {
	errno = op.result;
	return false;
    }

    while (op.result == SIOPENDING)
	ThrWaitHandle(op.event);

    if (op.result != 0)
    {
	errno = op.result;
	return false;
    }

    if (bytes_read != 0)
	*bytes_read = op.bytes;

    return true;
}
